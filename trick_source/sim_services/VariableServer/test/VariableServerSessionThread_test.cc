/******************************TRICK HEADER*************************************
PURPOSE:                     ( Tests for the VariableServerSessionThread class )
*******************************************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdexcept>
#include <chrono>
#include <future>
#include <thread>

#include "trick/VariableServer.hh"
#include "trick/ExecutiveException.hh"
#include "trick/message_type.h"

#include "trick/VariableServerSessionThread.hh"

#include "trick/Mock/MockMessagePublisher.hh"
#include "trick/Mock/MockVariableServerSession.hh"
#include "trick/Mock/MockClientConnection.hh"

using ::testing::Return;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Throw;
using ::testing::Const;
using ::testing::NiceMock;


// Set up default mock behavior

void setup_default_connection_mocks (MockClientConnection * connection) {
    // Starting the connection succeeds
    ON_CALL(*connection, start())
        .WillByDefault(Return(0));
    
    // We should always get a disconnect call
    ON_CALL(*connection, disconnect())
        .WillByDefault(Return(0));
}

void setup_default_session_mocks (MockVariableServerSession * session, bool command_exit = true) {
    ON_CALL(*session, handle_message())
        .WillByDefault(Return(0));

    ON_CALL(*session, copy_and_write_async())
        .WillByDefault(Return(0));

    ON_CALL(*session, get_pause())
        .WillByDefault(Return(false));
        
    ON_CALL(Const(*session), get_update_rate())
        .WillByDefault(Return(0.001));

    ON_CALL(*session, get_exit_cmd())
        .WillByDefault(Return(false));
}

/*
 Test Fixture.
 */
class VariableServerSessionThread_test : public ::testing::Test {
	protected:
        Trick::VariableServer * varserver;

        MockClientConnection * connection;
        NiceMock<MockVariableServerSession> * session;

        MockMessagePublisher message_publisher;

        VariableServerSessionThread_test()
        {
            varserver = new Trick::VariableServer;

            varserver->set_enabled(1);
            varserver->set_allow_connections(1);
            varserver->set_bypass_ip_check(1);

            Trick::VariableServerSessionThread::set_vs_ptr(varserver);

            // Set up mocks
            session = new  NiceMock<MockVariableServerSession>;
            connection = new MockClientConnection;
            setup_default_connection_mocks(connection);
            setup_default_session_mocks(session);
        }

        ~VariableServerSessionThread_test()
        {
            delete varserver;
            // session is owned and cleaned up by the VariableServerSessionThread
        }

        void SetUp() { }
        void TearDown() { }
};


// Helper functions for common cases of Mock expectations

void setup_normal_connection_expectations (MockClientConnection * connection) {
    // Starting the connection succeeds
    EXPECT_CALL(*connection, start())
        .Times(1)
        .WillOnce(Return(0));
    
    // We should always get a disconnect call
    EXPECT_CALL(*connection, disconnect())
        .Times(1);
}

void set_session_exit_after_some_loops(MockVariableServerSession * session) {
    EXPECT_CALL(*session, get_exit_cmd())
        .WillOnce(Return(false))
        .WillOnce(Return(false))
        .WillOnce(Return(false))
        .WillOnce(Return(false))
        .WillOnce(Return(false))
        .WillOnce(Return(true));
}

TEST_F(VariableServerSessionThread_test, connection_failure) {
    // ARRANGE

    // Starting the connection fails
    EXPECT_CALL(*connection, start())
        .Times(1)
        .WillOnce(Return(1));
    
    // Set up VariableServerSessionThread
    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    // ACT
    vst->create_thread();
    pthread_t id = vst->get_pthread_id();
    Trick::ConnectionStatus status = vst->wait_for_accept();
    vst->join_thread();

    // ASSERT
    EXPECT_EQ(status, Trick::ConnectionStatus::CONNECTION_FAIL);

    // There should be nothing in the VariableServer's thread list
    EXPECT_EQ(varserver->get_vst(id), (Trick::VariableServerSessionThread *) NULL);
    EXPECT_EQ(varserver->get_session(id), (Trick::VariableServerSession *) NULL);

    // The thread owns the session and the connection, so deleting it releases both.
    delete vst;
}


TEST_F(VariableServerSessionThread_test, DISABLED_exit_if_handle_message_fails) {

    // ARRANGE
    setup_normal_connection_expectations(connection);
    
    // Handle a message, but it fails
    EXPECT_CALL(*session, handle_message())
        .Times(1)
        .WillOnce(Return(-1));
    
        
    // Set up VariableServerSessionThread
    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    // ACT
    vst->create_thread();
    pthread_t id = vst->get_pthread_id();
    Trick::ConnectionStatus status = vst->wait_for_accept();
    ASSERT_EQ(status, Trick::ConnectionStatus::CONNECTION_SUCCESS);

    vst->join_thread();

    // ASSERT

    // There should be nothing in the VariableServer's thread list
    EXPECT_EQ(varserver->get_vst(id), (Trick::VariableServerSessionThread *) NULL);
    EXPECT_EQ(varserver->get_session(id), (Trick::VariableServerSession *) NULL);
}


TEST_F(VariableServerSessionThread_test, DISABLED_exit_if_write_fails) {

    // ARRANGE
    setup_normal_connection_expectations(connection);
    
    // Write out data
    EXPECT_CALL(*session, copy_and_write_async())
        .WillOnce(Return(-1));

        
    // Set up VariableServerSessionThread
    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    // ACT
    vst->create_thread();
    pthread_t id = vst->get_pthread_id();
    Trick::ConnectionStatus status = vst->wait_for_accept();
    ASSERT_EQ(status, Trick::ConnectionStatus::CONNECTION_SUCCESS);

    vst->join_thread();

    // ASSERT

    // There should be nothing in the VariableServer's thread list
    EXPECT_EQ(varserver->get_vst(id), (Trick::VariableServerSessionThread *) NULL);
    EXPECT_EQ(varserver->get_session(id), (Trick::VariableServerSession *) NULL);
}

TEST_F(VariableServerSessionThread_test, exit_commanded) {
    // ARRANGE
    setup_normal_connection_expectations(connection);
    set_session_exit_after_some_loops(session);

    // Set up VariableServerSessionThread
    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    // ACT
    vst->create_thread();
    pthread_t id = vst->get_pthread_id();
    Trick::ConnectionStatus status = vst->wait_for_accept();
    ASSERT_EQ(status, Trick::ConnectionStatus::CONNECTION_SUCCESS);

    // Runs for a few loops, then exits

    // Thread should shut down
    vst->join_thread();

    // ASSERT
    // There should be nothing in the VariableServer's thread list
    EXPECT_EQ(varserver->get_vst(id), (Trick::VariableServerSessionThread *) NULL);
    EXPECT_EQ(varserver->get_session(id), (Trick::VariableServerSession *) NULL);
}

TEST_F(VariableServerSessionThread_test, thread_cancelled) {
    // ARRANGE
    setup_normal_connection_expectations(connection);
    
    // Set up VariableServerSessionThread
    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));
    vst->create_thread();
    pthread_t id = vst->get_pthread_id();
    Trick::ConnectionStatus status = vst->wait_for_accept();
    ASSERT_EQ(status, Trick::ConnectionStatus::CONNECTION_SUCCESS);

    // Confirm that the session has been created
    Trick::VariableServerSession * vs_session = varserver->get_session(id);
    ASSERT_TRUE(vs_session == session);

    // ACT
    vst->cancel_thread();

    // Thread should shut down
    vst->join_thread();

    // ASSERT
    // There should be nothing in the VariableServer's thread list
    EXPECT_EQ(varserver->get_vst(id), (Trick::VariableServerSessionThread *) NULL);
    EXPECT_EQ(varserver->get_session(id), (Trick::VariableServerSession *) NULL);
}


TEST_F(VariableServerSessionThread_test, turn_session_log_on) {
    // ARRANGE
    setup_normal_connection_expectations(connection);
    set_session_exit_after_some_loops(session);

    varserver->set_var_server_log_on();

    // We expect a the session's log to be turned on
    EXPECT_CALL(*session, set_log(true))
        .Times(1);

    // Set up VariableServerSessionThread
    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    // ACT
    vst->create_thread();
    pthread_t id = vst->get_pthread_id();
    Trick::ConnectionStatus status = vst->wait_for_accept();
    ASSERT_EQ(status, Trick::ConnectionStatus::CONNECTION_SUCCESS);

    // Thread should shut down
    vst->join_thread();

    // ASSERT
    // There should be nothing in the VariableServer's thread list
    EXPECT_EQ(varserver->get_vst(id), (Trick::VariableServerSessionThread *) NULL);
    EXPECT_EQ(varserver->get_session(id), (Trick::VariableServerSession *) NULL);
}

TEST_F(VariableServerSessionThread_test, throw_trick_executive_exception) {
    // ARRANGE
    setup_normal_connection_expectations(connection);

    EXPECT_CALL(*session, get_exit_cmd())
        .WillRepeatedly(Return(false));

    // Set up VariableServerSessionThread
    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    EXPECT_CALL(*session, handle_message())
        .WillOnce(Throw(Trick::ExecutiveException(-1, __FILE__, __LINE__, "Trick::ExecutiveException Error message for testing")));

    EXPECT_CALL(message_publisher, publish(MSG_ERROR, _));

    // ACT
    vst->create_thread();
    pthread_t id = vst->get_pthread_id();
    Trick::ConnectionStatus status = vst->wait_for_accept();
    ASSERT_EQ(status, Trick::ConnectionStatus::CONNECTION_SUCCESS);

    // Thread should shut down
    vst->join_thread();

    // ASSERT
    // There should be nothing in the VariableServer's thread list
    EXPECT_EQ(varserver->get_vst(id), (Trick::VariableServerSessionThread *) NULL);
    EXPECT_EQ(varserver->get_session(id), (Trick::VariableServerSession *) NULL);
}

TEST_F(VariableServerSessionThread_test, throw_exception) {
    // ARRANGE
    setup_normal_connection_expectations(connection);

    EXPECT_CALL(*session, get_exit_cmd())
        .WillRepeatedly(Return(false));

    // Set up VariableServerSessionThread
    Trick::VariableServerSessionThread * vst = new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    EXPECT_CALL(*session, handle_message())
        .WillOnce(Throw(std::logic_error("Error message for testing")));
    EXPECT_CALL(message_publisher, publish(MSG_ERROR, _));


    // ACT
    vst->create_thread();
    pthread_t id = vst->get_pthread_id();
    Trick::ConnectionStatus status = vst->wait_for_accept();
    ASSERT_EQ(status, Trick::ConnectionStatus::CONNECTION_SUCCESS);

    // Thread should shut down
    vst->join_thread();

    // ASSERT
    // There should be nothing in the VariableServer's thread list
    EXPECT_EQ(varserver->get_vst(id), (Trick::VariableServerSessionThread *) NULL);
    EXPECT_EQ(varserver->get_session(id), (Trick::VariableServerSession *) NULL);
}


// Runs preload_checkpoint() on its own thread and reports whether it returned in time.
// A bounded wait is essential here: the bug this guards against is an unbounded one, so
// asserting on it directly would hang the suite instead of failing it.
static bool preload_checkpoint_completes(Trick::VariableServerSessionThread * vst,
                                         std::chrono::seconds budget)
{
    auto done = std::make_shared<std::promise<void>>();
    auto fut  = done->get_future();
    std::thread worker([vst, done] {
        vst->preload_checkpoint();
        done->set_value();
    });

    const bool completed = fut.wait_for(budget) == std::future_status::ready;
    if (completed) {
        worker.join();
        return true;
    }

    // The worker is wedged in an unbounded wait. Joining would wedge the suite too, but
    // detaching and returning would let the caller delete the thread object the worker
    // still holds -- turning a clean failure into a crash or a hang somewhere else.
    // Abort instead: the regression this guards is precisely an unbounded hang, so failing
    // loudly here is the honest outcome and leaves the process state intact for a trace.
    ADD_FAILURE() << "preload_checkpoint() did not return within "
                  << budget.count() << "s; aborting rather than leaving a wedged thread "
                  << "referencing an object the test is about to destroy";
    std::abort();
}


// Regression: suspending for a checkpoint must not wait on a session that has already
// exited. force_thread_to_pause() waits for an acknowledgement produced by test_pause(),
// and a session that disconnected, was told to exit, or failed a write will never call
// test_pause() again, so the wait had no terminal condition.
TEST_F(VariableServerSessionThread_test, preload_checkpoint_returns_when_session_has_exited) {
    // ARRANGE
    setup_normal_connection_expectations(connection);

    // The session exits on its first pass through the loop, after test_pause() has already
    // cleared the paused flag.
    EXPECT_CALL(*session, get_exit_cmd())
        .WillOnce(Return(true));

    Trick::VariableServerSessionThread * vst =
        new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    vst->create_thread();
    ASSERT_EQ(vst->wait_for_accept(), Trick::ConnectionStatus::CONNECTION_SUCCESS);

    // The session is definitively gone before we ask it to pause.
    vst->join_thread();

    // ACT / ASSERT
    EXPECT_TRUE(preload_checkpoint_completes(vst, std::chrono::seconds(10)))
        << "preload_checkpoint() waited for a pause acknowledgement from an exited session";

    delete vst;
}


// The ordinary path must keep working: a live session still pauses and resumes.
TEST_F(VariableServerSessionThread_test, preload_checkpoint_pauses_and_restarts_a_live_session) {
    // ARRANGE
    setup_normal_connection_expectations(connection);
    EXPECT_CALL(*connection, restart())
        .WillOnce(Return(0));

    // Keep the session alive until this test says otherwise. Counting loop iterations would
    // let it exit early under delayed scheduling, so the suspension would silently exercise
    // the exited path instead of the live one it is meant to cover.
    std::promise<void> may_exit;
    auto may_exit_future = may_exit.get_future();
    EXPECT_CALL(*session, get_exit_cmd())
        .WillRepeatedly(testing::Invoke([&may_exit_future] {
            return may_exit_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        }));

    // The live session is suspended and resumed, so it saves and restores its pause state.
    EXPECT_CALL(*session, get_pause())
        .WillRepeatedly(Return(false));

    Trick::VariableServerSessionThread * vst =
        new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    vst->create_thread();
    ASSERT_EQ(vst->wait_for_accept(), Trick::ConnectionStatus::CONNECTION_SUCCESS);

    // ACT / ASSERT
    EXPECT_TRUE(preload_checkpoint_completes(vst, std::chrono::seconds(10)))
        << "preload_checkpoint() failed to pause a live session";

    vst->restart();

    // Only now let the session leave.
    may_exit.set_value();

    vst->join_thread();
    delete vst;
}


// Regression for the concurrent window, not just the already-finished case.
//
// The session is parked inside handle_message() when suspension asks it to pause, and then
// leaves instead of acknowledging. The acknowledgement must mean teardown is *complete*:
// this test holds the session's teardown open (by blocking inside cleanup's disconnect())
// and asserts that suspension has not returned while it is still in progress. Publishing
// the terminal state before teardown lets suspension return early, so the resume phase can
// still find the session registered and restart one that suspension deliberately skipped.
TEST_F(VariableServerSessionThread_test, preload_checkpoint_waits_for_teardown_when_session_exits) {
    // ARRANGE
    EXPECT_CALL(*connection, start()).Times(1).WillOnce(Return(0));

    std::promise<void> parked, may_return;
    std::promise<void> tearing_down, may_finish_teardown;
    auto parked_f       = parked.get_future();
    auto may_return_f   = may_return.get_future();
    auto tearing_down_f = tearing_down.get_future();
    auto may_finish_f   = may_finish_teardown.get_future();

    EXPECT_CALL(*session, handle_message())
        .WillOnce(testing::Invoke([&] {
            parked.set_value();
            may_return_f.wait();
            return -1;                  // client disconnected: the loop breaks and exits
        }));

    // disconnect() runs inside cleanup(), i.e. inside the exiting thread's teardown.
    EXPECT_CALL(*connection, disconnect())
        .WillOnce(testing::Invoke([&] {
            tearing_down.set_value();
            may_finish_f.wait();
            return 0;
        }));

    Trick::VariableServerSessionThread * vst =
        new Trick::VariableServerSessionThread(std::unique_ptr<Trick::VariableServerSession>(session)) ;
    vst->set_connection(std::unique_ptr<Trick::ClientConnection>(connection));

    vst->create_thread();
    ASSERT_EQ(vst->wait_for_accept(), Trick::ConnectionStatus::CONNECTION_SUCCESS);
    ASSERT_EQ(parked_f.wait_for(std::chrono::seconds(10)), std::future_status::ready);

    // ACT
    auto done = std::make_shared<std::promise<void>>();
    auto suspended = done->get_future();
    std::thread suspender([vst, done] {
        vst->preload_checkpoint();
        done->set_value();
    });

    may_return.set_value();     // let the session leave instead of acknowledging

    // ASSERT
    // Teardown is now in flight and deliberately stalled.
    ASSERT_EQ(tearing_down_f.wait_for(std::chrono::seconds(10)), std::future_status::ready)
        << "the exiting session never reached teardown";

    EXPECT_EQ(suspended.wait_for(std::chrono::milliseconds(500)), std::future_status::timeout)
        << "suspension returned while the session was still tearing down; the resume phase "
           "could still have found it registered and restarted it";

    may_finish_teardown.set_value();

    EXPECT_EQ(suspended.wait_for(std::chrono::seconds(10)), std::future_status::ready)
        << "suspension did not complete once the session finished tearing down";
    suspender.join();

    vst->join_thread();
    delete vst;
}
