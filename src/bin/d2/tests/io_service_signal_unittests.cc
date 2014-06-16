// Copyright (C) 2014 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <d_test_stubs.h>
#include <d2/io_service_signal.h>

#include <gtest/gtest.h>

#include <queue>

namespace isc {
namespace d2 {

/// @brief Test fixture for testing the use of IOSignals.
///
/// This fixture is exercises IOSignaling as it is intended to be used in
/// an application in conjuction with util::SignalSet.
class IOSignalTest : public ::testing::Test {
public:
    /// @brief IOService instance to process IO.
    IOServicePtr io_service_;
    /// @brief Failsafe timer to ensure test(s) do not hang.
    isc::asiolink::IntervalTimer test_timer_;
    /// @brief Maximum time should be allowed to run.
    int test_time_ms_;

    /// @brief SignalSet object so we can catch real signals.
    util::SignalSetPtr signal_set_;

    /// @brief IOSignalQueue so we can generate IOSignals.
    IOSignalQueuePtr io_signal_queue_;

    /// @brief Vector to record the signal values received.
    std::vector<int> processed_signals_;
    /// @brief The number of signals that must be received to stop the test.
    int stop_at_count_;
    /// @brief Boolean which causes IOSignalHandler to throw if true.
    bool handler_throw_error_;

    /// @brief Constructor
    IOSignalTest() :
        io_service_(new asiolink::IOService()), test_timer_(*io_service_),
        test_time_ms_(0), signal_set_(),
        io_signal_queue_(new IOSignalQueue(io_service_)),
        processed_signals_(), stop_at_count_(0), handler_throw_error_(false) {
    }

    /// @brief Destructor
    ~IOSignalTest() {
        if (signal_set_) {
            signal_set_->clear();
        }

        // clear the on-receipt handler
        util::SignalSet::clearOnReceiptHandler();
    }

    /// @brief On-receipt signal handler used by unit tests.
    ///
    /// This function is registered with SignalSet as the "on-receipt" handler.
    /// When an OS signal is caught it schedules an IOSignal.
    ///
    /// @param signum Signal being handled.
    bool onReceiptHandler(int signum) {
        // Queue up a signal binging processSignal instance method as the
        // IOSignalHandler.
        io_signal_queue_->pushSignal(signum,
                                  boost::bind(&IOSignalTest::processSignal,
                                              this, _1));

        // Return true so SignalSet knows the signal has been consumed.
        return (true);
    }

    /// @brief Method used as the IOSignalHandler.
    ///
    /// Records the value of the given signal and checks if the desired
    /// number of signals have been received.  If so, the IOService is
    /// stopped which will cause IOService::run() to exit, returning control
    /// to the test.
    ///
    /// @param sequence_id id of the IOSignal received
    void processSignal(IOSignalId sequence_id) {
        // Pop the signal instance off the queue.  This should make us
        // the only one holding it, so when we leave it should be freed.
        IOSignalPtr signal = io_signal_queue_->popSignal(sequence_id);

        // Remember the signal we got.
        processed_signals_.push_back(signal->getSignum());

        // If the flag is on, force a throw to test error handling.
        if (handler_throw_error_) {
            handler_throw_error_ = false;
            isc_throw(BadValue, "processSignal throwing simulated error");
        }

        // If we've hit the number we want stop the IOService. This will cause
        // run to exit.
        if (processed_signals_.size() >= stop_at_count_) {
            io_service_->stop();
        }
    }

    /// @brief Sets the failsafe timer for the test to the given time.
    ///
    /// @param  test_time_ms maximum time in milliseconds the test should
    /// be allowed to run.
    void setTestTime(int test_time_ms) {
        // Fail safe shutdown
        test_time_ms_ = test_time_ms;
        test_timer_.setup(boost::bind(&IOSignalTest::testTimerHandler,
                                      this),
                          test_time_ms_, asiolink::IntervalTimer::ONE_SHOT);
    }

    /// @brief Failsafe timer expiration handler.
    void testTimerHandler() {
        io_service_->stop();
        FAIL() << "Test Time: " << test_time_ms_ << " expired";
    }
};

// Used for constuctor tests.
void dummyHandler(IOSignalId) {
}

// Tests IOSignal constructor.
TEST(IOSignal, construction) {
    IOServicePtr io_service(new asiolink::IOService());
    IOSignalPtr signal;

    // Verify that handler cannot be empty.
    ASSERT_THROW(signal.reset(new IOSignal(*io_service, SIGINT,
                                           IOSignalHandler())),
                 IOSignalError);

    // Verify constructor with valid arguments works.
    ASSERT_NO_THROW(signal.reset(new IOSignal(*io_service, SIGINT,
                                              dummyHandler)));
    // Verify sequence_id is 2, we burned 1 with the failed constructor.
    EXPECT_EQ(2, signal->getSequenceId());

    // Verify SIGINT is correct.
    EXPECT_EQ(SIGINT, signal->getSignum());
}

// Tests IOSignalQueue constructors and exercises queuing methods.
TEST(IOSignalQueue, constructionAndQueuing) {
    IOSignalQueuePtr queue;
    IOServicePtr io_service;

    // Verify constructing with an empty IOService will throw.
    ASSERT_THROW(queue.reset(new IOSignalQueue(io_service)), IOSignalError);

    // Verify valid construction works.
    io_service.reset(new asiolink::IOService());
    ASSERT_NO_THROW(queue.reset(new IOSignalQueue(io_service)));

    // Verify an empty handler is not allowed.
    ASSERT_THROW(queue->pushSignal(SIGINT, IOSignalHandler()),
                 IOSignalError);

    // Verify we can queue up a valid entry.
    IOSignalId sequence_id = queue->pushSignal(SIGINT, dummyHandler);

    // Verify we can pop the entry.
    IOSignalPtr signal = queue->popSignal(sequence_id);
    ASSERT_TRUE(signal);

    // Verify the one we popped is right.
    EXPECT_EQ(sequence_id, signal->getSequenceId());
    EXPECT_EQ(SIGINT, signal->getSignum());

    // Verify popping it again, throws.
    ASSERT_THROW(queue->popSignal(sequence_id), IOSignalError);
}

// Test the basic mechanics of IOSignal by handling one signal occurrence.
TEST_F(IOSignalTest, singleSignalTest) {
    // Set test fail safe.
    setTestTime(1000);

    // Register the onreceipt-handler with SignalSet.
    // We set this up to catch the actual signal.  The onreceipt handler
    // creates an IOSignal which should propagate the signal as a
    // IOService event.
    util::SignalSet::
    setOnReceiptHandler(boost::bind(&IOSignalTest::onReceiptHandler,
                                    this, _1));

    // Register to receive SIGINT.
    ASSERT_NO_THROW(signal_set_.reset(new util::SignalSet(SIGINT)));

    // Use TimedSignal to generate SIGINT 100 ms after we start IOService::run.
    TimedSignal sig_int(*io_service_, SIGINT, 100);

    // The first handler executed is the IOSignal's internal timer expirey
    // callback.
    io_service_->run_one();

    // The next handler executed is IOSignal's handler.
    io_service_->run_one();

    // Verify that we processed the signal.
    ASSERT_EQ(1, processed_signals_.size());

    // Now check that signal value is correct.
    EXPECT_EQ(SIGINT, processed_signals_[0]);
}


// Test verifies that signals can be delivered rapid-fire without falling over.
TEST_F(IOSignalTest, hammer) {
    // Set test fail safe.
    setTestTime(5000);

    // Register the onreceipt-handler with SignalSet, and register to receive
    // SIGINT.
    util::SignalSet::
    setOnReceiptHandler(boost::bind(&IOSignalTest::onReceiptHandler,
                                    this, _1));
    ASSERT_NO_THROW(signal_set_.reset(new util::SignalSet(SIGINT)));

    // Stop the test after 500 signals.
    stop_at_count_ = 500;

    // User a repeating TimedSignal so we should generate a signal every 1 ms
    // until we hit our stop count.
    TimedSignal sig_int(*io_service_, SIGINT, 1,
                        asiolink::IntervalTimer::REPEATING);

    // Start processing IO.  This should continue until we stop either by
    // hitting the stop count or if things go wrong, max test time.
    io_service_->run();

    // Verify we received the expected number of signals.
    EXPECT_EQ(stop_at_count_, processed_signals_.size());

    // Now check that each signal value is correct. This is sort of a silly
    // check but it does ensure things didn't go off the rails somewhere.
    for (int i = 0; i < processed_signals_.size(); ++i) {
        EXPECT_EQ(SIGINT, processed_signals_[i]);
    }
}

// Verifies that handler exceptions are caught.
TEST_F(IOSignalTest, handlerThrow) {
    // Set test fail safe.
    setTestTime(1000);

    // Register the onreceipt-handler with SignalSet, and register to
    // receive SIGINT.
    util::SignalSet::
    setOnReceiptHandler(boost::bind(&IOSignalTest::onReceiptHandler,
                                    this, _1));
    ASSERT_NO_THROW(signal_set_.reset(new util::SignalSet(SIGINT)));

    // Set the stop after we've done at least 1 all the way through.
    stop_at_count_ = 1;

    // Use TimedSignal to generate SIGINT after we start IOService::run.
    TimedSignal sig_int(*io_service_, SIGINT, 100,
                        asiolink::IntervalTimer::REPEATING);

    // Set the test flag to cause the handler to throw an exception.
    handler_throw_error_ = true;

    // Start processing IO.  This should fail with the handler exception.
    ASSERT_NO_THROW(io_service_->run());

    // Verify that the we hit the throw block.  The flag will be false
    // we will have skipped the stop count check so number signals processed
    // is stop_at_count_ + 1.
    EXPECT_FALSE(handler_throw_error_);
    EXPECT_EQ(stop_at_count_ + 1, processed_signals_.size());
}

// Verifies that we can handle a mixed set of signals.
TEST_F(IOSignalTest, mixedSignals) {
    // Set test fail safe.
    setTestTime(1000);

    // Register the onreceipt-handler with SignalSet, and register to
    // receive SIGINT, SIGUSR1, and SIGUSR2.
    util::SignalSet::
    setOnReceiptHandler(boost::bind(&IOSignalTest::onReceiptHandler,
                                    this, _1));
    ASSERT_NO_THROW(signal_set_.reset(new util::SignalSet(SIGINT, SIGUSR1,
                                      SIGUSR2)));

    // Stop the test after 21 signals.
    stop_at_count_ = 21;

    // User a repeating TimedSignal so we should generate a signal every 1 ms
    // until we hit our stop count.
    TimedSignal sig_1(*io_service_, SIGINT, 1,
                      asiolink::IntervalTimer::REPEATING);
    TimedSignal sig_2(*io_service_, SIGUSR1, 1,
                      asiolink::IntervalTimer::REPEATING);
    TimedSignal sig_3(*io_service_, SIGUSR2, 1,
                      asiolink::IntervalTimer::REPEATING);

    // Start processing IO.  This should continue until we stop either by
    // hitting the stop count or if things go wrong, max test time.
    io_service_->run();

    // Verify we received the expected number of signals.
    ASSERT_EQ(stop_at_count_, processed_signals_.size());

    // If the underlying implmemeation is orderly, the signals should have
    // been processed in sets of three: SIGINT, SIGUSR, SIGUSR
    // It is conceivable under some OS's that they might not occur in this
    // order.
    for (int i = 0; i < 21; i += 3) {
        EXPECT_EQ(SIGINT, processed_signals_[i]);
        EXPECT_EQ(SIGUSR1, processed_signals_[i+1]);
        EXPECT_EQ(SIGUSR2, processed_signals_[i+2]);
    }
}


}; // end of isc::d2 namespace
}; // end of isc namespace
