/********************************************************************
 * test_qofinstance.c: GLib g_test test suite for qofinstance.	    *
 * Copyright 2011 John Ralls <jralls@ceridwen.us>		    *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
\********************************************************************/

#include <glib.h>
#include <guid.hpp>

extern "C"
{
#include <config.h>
#include <unittest-support.h>
#include "../qof.h"
}
#include "../qof-backend.hpp"
#include "../kvp-frame.hpp"
static const gchar *suitename = "/qof/qofinstance";
extern "C" void test_suite_qofinstance ( void );
static gchar* error_message;
static gboolean is_called;

static struct
{
    QofInstance *m_inst = nullptr;
    QofBackend *m_be = nullptr;

    bool m_commit_called = false;
    bool m_commit_with_err_called = false;
    bool m_on_error_called = false;
    bool m_on_free_called = false;
    bool m_on_done_called = false;
    QofBackendError m_err = ERR_BACKEND_NO_ERR;
} commit_test;


class QofInstMockBackend : public QofBackend
{
public:
    QofInstMockBackend() : m_qof_error{ERR_BACKEND_NO_ERR} {
        commit_test.m_be = this;
    }
    void session_begin(QofSession* sess, const char* uri,
                       SessionOpenMode mode) override {}
    void session_end() override {}
    void load(QofBook*, QofBackendLoadType) override {}
    void sync(QofBook* book) override {}
    void safe_sync(QofBook* book) override {}
    void begin(QofInstance* inst) override {
        g_assert(inst);
        g_assert(QOF_IS_INSTANCE(inst));
        commit_test.m_inst = inst;
    }
    void set_error(QofBackendError err) {
        QofBackend::set_error(err);
        commit_test.m_err = err;
    }
    void commit(QofInstance* inst) override {
        g_assert( inst );
        g_assert( QOF_IS_INSTANCE( inst ) );
        g_assert( commit_test.m_inst == inst );
        g_assert( commit_test.m_be == this );
        commit_test.m_commit_called = true;
        if (qof_instance_is_dirty(inst))
            qof_instance_mark_clean(inst);
        set_error(m_qof_error);

    }
    void rollback(QofInstance* inst) override {}
    void inject_error(QofBackendError err) {
        m_qof_error = err;
    }
private:
    QofBackendError m_qof_error;

};

typedef struct
{
    QofInstance *inst;
} Fixture;

static void
on_error(QofInstance* inst, QofBackendError err) {
    g_assert( inst );
    g_assert( QOF_IS_INSTANCE( inst ) );
    g_assert( commit_test.m_err == err );
    commit_test.m_on_error_called = true;
}
static void
on_done(QofInstance* inst) {
    g_assert( inst );
    g_assert( QOF_IS_INSTANCE( inst ) );
    g_assert( commit_test.m_inst == inst );
    commit_test.m_on_done_called = true;
}
static void
on_free(QofInstance* inst) {
    g_assert( inst );
    g_assert( QOF_IS_INSTANCE( inst ) );
    g_assert(commit_test.m_inst == inst );
    commit_test.m_on_free_called = true;
}

/* use g_free on error_message after this function been called */
static gboolean
fatal_handler ( const char * log_domain,
                GLogLevelFlags log_level,
                const gchar *msg,
                gpointer user_data)
{
    error_message = (gchar *) g_strdup( msg );
    return FALSE;
}

static void
setup( Fixture *fixture, gconstpointer pData )
{
    fixture->inst = static_cast<QofInstance*>(g_object_new(QOF_TYPE_INSTANCE, NULL));
}

static void
teardown( Fixture *fixture, gconstpointer pData )
{
    g_object_unref(fixture->inst);
}

static void
test_instance_set_get_book( Fixture *fixture, gconstpointer pData )
{
    QofBook *book;

    /* set up */
    book = qof_book_new();

    g_assert( QOF_IS_INSTANCE( fixture->inst ) );

    g_test_message( "Setting and getting book" );
    qof_instance_set_book( fixture->inst, book );
    g_assert( book == qof_instance_get_book( fixture->inst ) );

    g_test_message( "Getting book when instance is null" );
    g_assert( qof_instance_get_book( NULL ) == NULL );

    /* Clean up */
    qof_book_destroy( book );
}

static void
test_instance_set_get_guid( Fixture *fixture, gconstpointer pData )
{
    GncGUID *gncGuid;

    /* on null instance deprecated getter returns empty guid
     * while instance_get_guid returns null
     */
    g_assert( !qof_instance_get_guid( NULL ) );
    g_assert( qof_entity_get_guid( NULL ) == guid_null() );

    /* set up */
    gncGuid = guid_new();
    g_assert( QOF_IS_INSTANCE( fixture->inst ) );
    g_assert( gncGuid );

    /* guid already exists after instance init */
    g_test_message( "Setting new guid" );
    g_assert( qof_instance_get_guid( fixture->inst ) );
    g_assert( !guid_equal( gncGuid, qof_instance_get_guid( fixture->inst ) ) );
    qof_instance_set_guid( fixture->inst, gncGuid );
    g_assert( guid_equal( gncGuid, qof_instance_get_guid( fixture->inst ) ) );
    g_assert( guid_equal( gncGuid, qof_entity_get_guid( fixture->inst ) ) );

    /* Clean up */
    guid_free( gncGuid );
}

static void
test_instance_new_destroy( void )
{
    /* qofinstance var */
    QofInstance *inst;
    QofInstanceClass *klass;
    /* test var */
    Time64 *time_priv;
    const char *msg1 = "qof_instance_get_collection: assertion 'QOF_IS_INSTANCE(ptr)' failed";
    const char *msg2 = "qof_instance_get_editlevel: assertion 'QOF_IS_INSTANCE(ptr)' failed";
    const char *msg3 = "qof_instance_get_destroying: assertion 'QOF_IS_INSTANCE(ptr)' failed";
    const char *msg4 = "qof_instance_get_dirty_flag: assertion 'QOF_IS_INSTANCE(ptr)' failed";
    const char *log_domain = "qof";
    auto loglevel = static_cast<GLogLevelFlags>(G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_FATAL);
    auto check = test_error_struct_new(log_domain, loglevel, msg1);

    g_test_message( "Testing qofinstance object initialization" );
    inst = static_cast<QofInstance*>(g_object_new(QOF_TYPE_INSTANCE, NULL));
    g_assert( QOF_IS_INSTANCE( inst ) );
    /* test class fields */
    klass = QOF_INSTANCE_GET_CLASS( inst );
    g_assert( QOF_IS_INSTANCE_CLASS( klass ) );
    g_assert( klass->get_display_name == NULL );
    /* testing initial values */
    g_assert( qof_instance_get_guid( inst ) );
    g_assert( !qof_instance_get_collection( inst ) );
    g_assert( qof_instance_get_book( inst ) == NULL );
    g_assert( inst->kvp_data );
    g_object_get( inst, "last-update", &time_priv, NULL);
    g_assert_cmpint( time_priv->t, == , 0 );
    g_assert_cmpint( qof_instance_get_editlevel( inst ), == , 0 );
    g_assert( !qof_instance_get_destroying( inst ) );
    g_assert( !qof_instance_get_dirty_flag( inst ) );
    g_assert( qof_instance_get_infant( inst ) );
    g_assert_cmpint( qof_instance_get_version( inst ), == , 0 );
    g_assert_cmpint( qof_instance_get_version_check( inst ), == , 0 );
    g_assert_cmpint( qof_instance_get_idata( inst ), == , 0 );

    g_test_message( "Testing object destruction" );
    g_object_unref( inst );
    g_free( error_message );
    test_error_struct_free(check);
}

static void
test_instance_init_data( void )
{
    QofInstance *inst;
    QofIdType test_type = "test type";
    QofBook *book;
    QofCollection *col;
    const GncGUID *gncguid;
    char guid_id_before[GUID_ENCODING_LENGTH + 1];
    char guid_id_after[GUID_ENCODING_LENGTH + 1];

    /* set up */
    inst = static_cast<QofInstance*>(g_object_new( QOF_TYPE_INSTANCE, NULL ));
    g_assert( QOF_IS_INSTANCE( inst ) );
    book = qof_book_new();
    g_assert( QOF_IS_BOOK( book ) );
    /* set fatal handler */
    g_test_log_set_fatal_handler ( ( GTestLogFatalFunc )fatal_handler, NULL );

    g_test_message( "Running test with correct initial data" );
    gncguid = qof_instance_get_guid( inst );
    g_assert( gncguid );
    guid_to_string_buff( gncguid, guid_id_before );
    g_assert( qof_instance_get_book( inst ) == NULL );
    g_assert( qof_instance_get_collection( inst ) == NULL );
    /* run init */
    qof_instance_init_data( inst, test_type, book );

    g_assert( qof_instance_get_book( inst ) == book );
    guid_to_string_buff( gncguid, guid_id_after );
    g_assert_cmpstr( guid_id_before, != , guid_id_after );
    g_assert( qof_instance_get_collection( inst ) != NULL );
    col = qof_book_get_collection( book, test_type );
    g_assert( col );
    g_assert( col == qof_instance_get_collection( inst ) );
    g_assert_cmpstr( inst->e_type, == , test_type );
    g_assert( qof_collection_lookup_entity( qof_instance_get_collection( inst ), gncguid ) == inst );

    /* clean up */
    g_object_unref( inst );
    qof_book_destroy( book );
}

static void
test_instance_get_set_slots( Fixture *fixture, gconstpointer pData )
{
    KvpFrame *kvp_frame, *kvp_frame2;

    /* set up */
    g_assert( fixture->inst );
    kvp_frame = qof_instance_get_slots( fixture->inst );
    g_assert( kvp_frame );

    g_test_message( "Test when kvp frame is the same" );
    qof_instance_set_slots( fixture->inst, kvp_frame );
    g_assert( kvp_frame == qof_instance_get_slots( fixture->inst ) );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) );

    g_test_message( "Test when kvp frame is not the same" );
    kvp_frame2 = new KvpFrame;
    g_assert( kvp_frame != kvp_frame2 );
    qof_instance_set_slots( fixture->inst, kvp_frame2 );
    g_assert( kvp_frame2 == qof_instance_get_slots( fixture->inst ) );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) );

    g_test_message( "Test when kvp frame is null" );
    qof_instance_set_slots( fixture->inst, NULL );
    g_assert( NULL == qof_instance_get_slots( fixture->inst ) );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) );

}

static void
test_instance_version_cmp( void )
{
    QofInstance *left, *right;
    int result;
    time64 time_left = 0, time_right = 1;

    /* set up*/
    left = static_cast<QofInstance*>(g_object_new( QOF_TYPE_INSTANCE, NULL ));
    right = static_cast<QofInstance*>(g_object_new( QOF_TYPE_INSTANCE, NULL ));

    g_test_message( "Test both null" );
    result = qof_instance_version_cmp( NULL, NULL );
    g_assert_cmpint( result, == , 0 );

    g_test_message( "Test left null" );
    result = qof_instance_version_cmp( NULL, right );
    g_assert_cmpint( result, == , -1 );

    g_test_message( "Test right null" );
    result = qof_instance_version_cmp( left, NULL );
    g_assert_cmpint( result, == , 1 );

    g_test_message( "Test left tv_sec lesser than right" );
    qof_instance_set_last_update( left, time_left );
    qof_instance_set_last_update( right, time_right );
    result = qof_instance_version_cmp( left, right );
    g_assert_cmpint( result, == , -1 );

    g_test_message( "Test right tv_sec lesser than left" );
    time_left = 1;
    time_right = 0;
    qof_instance_set_last_update( left, time_left );
    qof_instance_set_last_update( right, time_right );
    result = qof_instance_version_cmp( left, right );
    g_assert_cmpint( result, == , 1 );

    g_test_message( "Test both equal" );
    time_left = 1;
    time_right = 1;
    qof_instance_set_last_update( left, time_left );
    qof_instance_set_last_update( right, time_right );
    result = qof_instance_version_cmp( left, right );
    g_assert_cmpint( result, == , 0 );

    /* clear */
    g_object_unref( left );
    g_object_unref( right );
}

static void
test_instance_get_set_dirty( Fixture *fixture, gconstpointer pData )
{
    QofIdType type = "test type";
    QofCollection *col;

    /* setup */
    col = qof_collection_new ( type );
    qof_instance_set_collection( fixture->inst, col );
    g_assert( qof_instance_get_collection( fixture->inst ) );

    g_test_message( "Test get_dirty on empty instance returns false" );
    g_assert( qof_instance_get_dirty( NULL ) == FALSE );

    g_test_message( "Test dirty" );
    g_assert( !qof_instance_get_dirty_flag( fixture->inst ) );
    g_assert( !qof_collection_is_dirty( col ) );
    g_assert( !qof_instance_get_dirty( fixture->inst ) );
    qof_instance_set_dirty( fixture->inst );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) );
    g_assert( !qof_collection_is_dirty( col ) );
    g_assert( qof_instance_get_dirty( fixture->inst ) );

    /* clean up */
    qof_instance_set_collection( fixture->inst, NULL );
    qof_collection_destroy( col );
}

/* mock display name function */
static gchar*
mock_get_display_name(const QofInstance* inst)
{
    gchar *display_name;

    g_assert( inst );
    g_assert( QOF_INSTANCE_GET_CLASS( inst )->get_display_name == mock_get_display_name );
    is_called = TRUE;
    display_name = g_strdup_printf("Mock display name %p", inst );
    return display_name;
}

static void
test_instance_display_name( Fixture *fixture, gconstpointer pData )
{
    QofIdType type = "test type";
    QofCollection *col;
    gchar *display_name, *default_display_name, *mock_display_name;

    /* setup */
    g_assert( fixture->inst );
    is_called = FALSE;
    col = qof_collection_new ( type );
    g_assert( col );
    qof_instance_set_collection( fixture->inst, col );
    g_assert( qof_instance_get_collection( fixture->inst ) );
    default_display_name = g_strdup_printf( "Object %s %p", type, fixture->inst );
    mock_display_name = g_strdup_printf( "Mock display name %p", fixture->inst );

    g_test_message( "Test instance when display name not set" );
    g_assert( QOF_INSTANCE_GET_CLASS( fixture->inst )->get_display_name == NULL );
    display_name = qof_instance_get_display_name( fixture->inst );
    g_assert( !is_called );
    g_assert_cmpstr( display_name, == , default_display_name );
    g_free( display_name );

    g_test_message( "Test instance when display name is set" );
    QOF_INSTANCE_GET_CLASS( fixture->inst )->get_display_name = mock_get_display_name;
    display_name = qof_instance_get_display_name( fixture->inst );
    g_assert( is_called );
    g_assert_cmpstr( display_name, == , mock_display_name );
    g_free( display_name );

    /* clean up */
    g_free( default_display_name );
    g_free( mock_display_name );
    qof_instance_set_collection( fixture->inst, NULL );
    qof_collection_destroy( col );
}

static void
mock_backend_begin( QofBackend *be, QofInstance *inst )
{
    g_assert( be );
    g_assert( inst );
    g_assert_cmpstr( inst->e_type, == , "test type" );
    is_called = TRUE;
}

static void
test_instance_begin_edit( Fixture *fixture, gconstpointer pData )
{
    QofBook *book;
    gboolean result;

    /* setup */
    auto be = new QofInstMockBackend;
    g_assert( be );
    book = qof_book_new();
    g_assert( book );
    g_assert( QOF_IS_BOOK( book ) );
    qof_book_set_backend( book, be );
    g_assert( fixture->inst );
    fixture->inst->e_type = "test type";
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) == FALSE );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , 0 );

    g_test_message( "Test when instance is null" );
    result = qof_begin_edit( NULL );
    g_assert( result == FALSE );

    g_test_message( "Test when instance's editlevel is >= 1" );
    qof_instance_increase_editlevel( fixture->inst );
    result = qof_begin_edit( fixture->inst );
    g_assert( result == FALSE );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , 2 );

    g_test_message( "Test when instance's editlevel is <= 0 and backend not set" );
    qof_instance_reset_editlevel( fixture->inst );
    result = qof_begin_edit( fixture->inst );
    g_assert( result == TRUE );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , 1 );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) == TRUE );

    g_test_message( "Test when instance's editlevel is <= 0 and backend is set" );
    result = FALSE;
    qof_instance_reset_editlevel( fixture->inst );
    qof_instance_set_dirty_flag( fixture->inst, FALSE );
    qof_instance_set_book( fixture->inst, book );
    result = qof_begin_edit( fixture->inst );
    g_assert( result == TRUE );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , 1 );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) == FALSE );

    /* clean up */
    qof_book_set_backend( book, NULL );
    qof_book_destroy( book );
    delete be;

}

static void
test_instance_commit_edit( Fixture *fixture, gconstpointer pData )
{
    gboolean result;
    const gchar *msg = "[qof_commit_edit()] unbalanced call - resetting (was -2)";
    const gchar *log_domain = "qof.engine";
    auto loglevel = static_cast<GLogLevelFlags>(G_LOG_LEVEL_CRITICAL | G_LOG_FLAG_FATAL);
    auto check = test_error_struct_new(log_domain, loglevel, msg);

    g_test_message( "Test when instance set to null" );
    result = qof_commit_edit( NULL );
    g_assert( !result );

    g_test_message( "Test when instance's editlevel >= 2" );
    qof_instance_increase_editlevel( fixture->inst );
    qof_instance_increase_editlevel( fixture->inst );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , 2 );
    result = qof_commit_edit( fixture->inst );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , 1 );
    g_assert( !result );

    g_test_message( "Test when instance's editlevel = 1" );
    result = qof_commit_edit( fixture->inst );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , 0 );
    g_assert( result );

    g_test_message( "Test when instance's editlevel < 0" );
    g_test_log_set_fatal_handler ( ( GTestLogFatalFunc )fatal_handler, NULL );
    auto hdlr = g_log_set_handler (log_domain, loglevel,
                                   (GLogFunc)test_checked_handler, check);
    qof_instance_decrease_editlevel( fixture->inst );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , -1 );
    result = qof_commit_edit( fixture->inst );
    g_assert_cmpint( qof_instance_get_editlevel( fixture->inst ), == , 0 );
    g_assert_cmpstr( error_message, == , "[qof_commit_edit()] unbalanced call - resetting (was -2)" );
    g_free( error_message );
    g_log_remove_handler (log_domain, hdlr);
    test_error_struct_free(check);
}

/* backend commit test start */


static void
test_instance_commit_edit_part2( Fixture *fixture, gconstpointer pData )
{
    QofBook *book;
    gboolean result;

    /* setup */
    auto be = new QofInstMockBackend;
    g_assert( be );
    book = qof_book_new();
    g_assert( book );
    g_assert( QOF_IS_BOOK( book ) );
    qof_book_set_backend( book, be );

    /* init */
    result = FALSE;
    commit_test.m_commit_called = false;
    commit_test.m_commit_with_err_called = false;
    commit_test.m_on_error_called = false;
    commit_test.m_on_free_called = false;
    commit_test.m_on_done_called = false;
    commit_test.m_inst = fixture->inst;
    commit_test.m_be = be;
    qof_instance_set_dirty_flag( fixture->inst, TRUE );

    g_test_message( "Test when instance's backend not set, callbacks not set" );
    g_assert( qof_instance_get_infant( fixture->inst ) );
    g_assert( !qof_instance_get_destroying( fixture->inst ) );
    result = qof_commit_edit_part2( fixture->inst, NULL, NULL, NULL );
    g_assert( result );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) );
    g_assert( qof_instance_get_infant( fixture->inst ) );
    g_assert( !commit_test.m_commit_called );
    g_assert( !commit_test.m_commit_with_err_called );
    g_assert( !commit_test.m_on_error_called );
    g_assert( !commit_test.m_on_free_called );
    g_assert( !commit_test.m_on_done_called );

    g_test_message( "Test when instance's backend not set, do_free is false" );
    qof_instance_set_destroying( fixture->inst, TRUE );
    result = qof_commit_edit_part2( fixture->inst, on_error, on_done, on_free );
    g_assert( result );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) );
    g_assert( qof_instance_get_infant( fixture->inst ) );
    g_assert( !commit_test.m_commit_called );
    g_assert( !commit_test.m_commit_with_err_called );
    g_assert( !commit_test.m_on_error_called );
    g_assert( commit_test.m_on_free_called );
    g_assert( !commit_test.m_on_done_called );

    g_test_message( "Test when instance's backend not set, do_free is false" );
    qof_instance_set_destroying( fixture->inst, FALSE );
    commit_test.m_on_free_called = false;
    result = qof_commit_edit_part2( fixture->inst, on_error, on_done, on_free );
    g_assert( result );
    g_assert( qof_instance_get_dirty_flag( fixture->inst ) );
    g_assert( qof_instance_get_infant( fixture->inst ) );
    g_assert( !commit_test.m_commit_called );
    g_assert( !commit_test.m_commit_with_err_called );
    g_assert( !commit_test.m_on_error_called );
    g_assert( !commit_test.m_on_free_called );
    g_assert( commit_test.m_on_done_called );

    g_test_message( "Test when instance's backend is set, all cb set, no error produced" );
    qof_instance_set_book( fixture->inst, book );
    qof_instance_set_destroying( fixture->inst, FALSE );
    commit_test.m_on_done_called = false;
    result = qof_commit_edit_part2( fixture->inst, on_error, on_done, on_free );
    g_assert( result );
    g_assert( !qof_instance_get_dirty_flag( fixture->inst ) );
    g_assert( !qof_instance_get_infant( fixture->inst ) );
    g_assert( commit_test.m_commit_called );
    g_assert( !commit_test.m_commit_with_err_called );
    g_assert( !commit_test.m_on_error_called );
    g_assert( !commit_test.m_on_free_called );
    g_assert( commit_test.m_on_done_called );

    g_test_message( "Test when instance's backend is set, all cb set, error produced" );
    commit_test.m_commit_called = false;
    commit_test.m_on_done_called = false;
    be->inject_error(ERR_BACKEND_NO_HANDLER);
    qof_instance_set_dirty_flag( fixture->inst, TRUE );
    qof_instance_set_destroying( fixture->inst, TRUE );
    result = qof_commit_edit_part2( fixture->inst, on_error, on_done, on_free );
    g_assert( !result );
    g_assert( !qof_instance_get_dirty_flag( fixture->inst ) );
    g_assert( !qof_instance_get_destroying( fixture->inst ) );
    g_assert( commit_test.m_commit_called );
    g_assert( commit_test.m_on_error_called );
    g_assert( !commit_test.m_on_free_called );
    g_assert( !commit_test.m_on_done_called );

    /* clean up */
    qof_book_set_backend( book, NULL );
    qof_book_destroy( book );
    delete be;
}

/* backend commit test end */

extern "C" void
test_suite_qofinstance ( void )
{
    GNC_TEST_ADD( suitename, "set get book", Fixture, NULL, setup, test_instance_set_get_book, teardown );
    GNC_TEST_ADD( suitename, "set get guid", Fixture, NULL, setup, test_instance_set_get_guid, teardown );
    GNC_TEST_ADD_FUNC( suitename, "instance new and destroy", test_instance_new_destroy );
    GNC_TEST_ADD_FUNC( suitename, "init data", test_instance_init_data );
    GNC_TEST_ADD( suitename, "get set slots", Fixture, NULL, setup, test_instance_get_set_slots, teardown );
    GNC_TEST_ADD_FUNC( suitename, "version compare", test_instance_version_cmp );
    GNC_TEST_ADD( suitename, "get set dirty", Fixture, NULL, setup, test_instance_get_set_dirty, teardown );
    GNC_TEST_ADD( suitename, "display name", Fixture, NULL, setup, test_instance_display_name, teardown );
    GNC_TEST_ADD( suitename, "begin edit", Fixture, NULL, setup, test_instance_begin_edit, teardown );
    GNC_TEST_ADD( suitename, "commit edit", Fixture, NULL, setup, test_instance_commit_edit, teardown );
    GNC_TEST_ADD( suitename, "commit edit part 2", Fixture, NULL, setup, test_instance_commit_edit_part2, teardown );
}
