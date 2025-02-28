/*
 *  PSA crypto layer on top of Mbed TLS crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "common.h"

#if defined(MBEDTLS_PSA_CRYPTO_C)

#include "psa_crypto_service_integration.h"
#include "psa/crypto.h"

#include "psa_crypto_core.h"
#include "psa_crypto_slot_management.h"
#include "psa_crypto_storage.h"
#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
#include "psa_crypto_se.h"
#endif

#include <stdlib.h>
#include <string.h>
#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#define vdb_mbedtls_calloc calloc
#define vdb_mbedtls_free   free
#endif

#define ARRAY_LENGTH( array ) ( sizeof( array ) / sizeof( *( array ) ) )

typedef struct
{
    psa_key_slot_t key_slots[PSA_KEY_SLOT_COUNT];
    unsigned key_slots_initialized : 1;
} psa_global_data_t;

static psa_global_data_t global_data;

psa_status_t psa_validate_key_id(
    mbedtls_svc_key_id_t key, int vendor_ok )
{
    psa_key_id_t key_id = MBEDTLS_SVC_KEY_ID_GET_KEY_ID( key );

    if( ( PSA_KEY_ID_USER_MIN <= key_id ) &&
        ( key_id <= PSA_KEY_ID_USER_MAX ) )
        return( PSA_SUCCESS );

    if( vendor_ok &&
        ( PSA_KEY_ID_VENDOR_MIN <= key_id ) &&
        ( key_id <= PSA_KEY_ID_VENDOR_MAX ) )
        return( PSA_SUCCESS );

    return( PSA_ERROR_INVALID_HANDLE );
}

/** Get the description in memory of a key given its identifier and lock it.
 *
 * The descriptions of volatile keys and loaded persistent keys are
 * stored in key slots. This function returns a pointer to the key slot
 * containing the description of a key given its identifier.
 *
 * The function searches the key slots containing the description of the key
 * with \p key identifier. The function does only read accesses to the key
 * slots. The function does not load any persistent key thus does not access
 * any storage.
 *
 * For volatile key identifiers, only one key slot is queried as a volatile
 * key with identifier key_id can only be stored in slot of index
 * ( key_id - #PSA_KEY_ID_VOLATILE_MIN ).
 *
 * On success, the function locks the key slot. It is the responsibility of
 * the caller to unlock the key slot when it does not access it anymore.
 *
 * \param key           Key identifier to query.
 * \param[out] p_slot   On success, `*p_slot` contains a pointer to the
 *                      key slot containing the description of the key
 *                      identified by \p key.
 *
 * \retval #PSA_SUCCESS
 *         The pointer to the key slot containing the description of the key
 *         identified by \p key was returned.
 * \retval #PSA_ERROR_INVALID_HANDLE
 *         \p key is not a valid key identifier.
 * \retval #PSA_ERROR_DOES_NOT_EXIST
 *         There is no key with key identifier \p key in the key slots.
 */
static psa_status_t psa_get_and_lock_key_slot_in_memory(
    mbedtls_svc_key_id_t key, psa_key_slot_t **p_slot )
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_id_t key_id = MBEDTLS_SVC_KEY_ID_GET_KEY_ID( key );
    size_t slot_idx;
    psa_key_slot_t *slot = NULL;

    if( psa_key_id_is_volatile( key_id ) )
    {
        slot = &global_data.key_slots[ key_id - PSA_KEY_ID_VOLATILE_MIN ];

        /*
         * Check if both the PSA key identifier key_id and the owner
         * identifier of key match those of the key slot.
         *
         * Note that, if the key slot is not occupied, its PSA key identifier
         * is equal to zero. This is an invalid value for a PSA key identifier
         * and thus cannot be equal to the valid PSA key identifier key_id.
         */
        status = vdb_mbedtls_svc_key_id_equal( key, slot->attr.id ) ?
                 PSA_SUCCESS : PSA_ERROR_DOES_NOT_EXIST;
    }
    else
    {
        status = psa_validate_key_id( key, 1 );
        if( status != PSA_SUCCESS )
            return( status );

        for( slot_idx = 0; slot_idx < PSA_KEY_SLOT_COUNT; slot_idx++ )
        {
            slot = &global_data.key_slots[ slot_idx ];
            if( vdb_mbedtls_svc_key_id_equal( key, slot->attr.id ) )
                break;
        }
        status = ( slot_idx < PSA_KEY_SLOT_COUNT ) ?
                 PSA_SUCCESS : PSA_ERROR_DOES_NOT_EXIST;
    }

    if( status == PSA_SUCCESS )
    {
        status = psa_lock_key_slot( slot );
        if( status == PSA_SUCCESS )
            *p_slot = slot;
    }

    return( status );
}

psa_status_t psa_initialize_key_slots( void )
{
    /* Nothing to do: program startup and psa_wipe_all_key_slots() both
     * guarantee that the key slots are initialized to all-zero, which
     * means that all the key slots are in a valid, empty state. */
    global_data.key_slots_initialized = 1;
    return( PSA_SUCCESS );
}

void psa_wipe_all_key_slots( void )
{
    size_t slot_idx;

    for( slot_idx = 0; slot_idx < PSA_KEY_SLOT_COUNT; slot_idx++ )
    {
        psa_key_slot_t *slot = &global_data.key_slots[ slot_idx ];
        slot->lock_count = 1;
        (void) psa_wipe_key_slot( slot );
    }
    global_data.key_slots_initialized = 0;
}

psa_status_t psa_get_empty_key_slot( psa_key_id_t *volatile_key_id,
                                     psa_key_slot_t **p_slot )
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t slot_idx;
    psa_key_slot_t *selected_slot, *unlocked_persistent_key_slot;

    if( ! global_data.key_slots_initialized )
    {
        status = PSA_ERROR_BAD_STATE;
        goto error;
    }

    selected_slot = unlocked_persistent_key_slot = NULL;
    for( slot_idx = 0; slot_idx < PSA_KEY_SLOT_COUNT; slot_idx++ )
    {
        psa_key_slot_t *slot = &global_data.key_slots[ slot_idx ];
        if( ! psa_is_key_slot_occupied( slot ) )
        {
            selected_slot = slot;
            break;
        }

        if( ( unlocked_persistent_key_slot == NULL ) &&
            ( ! PSA_KEY_LIFETIME_IS_VOLATILE( slot->attr.lifetime ) ) &&
            ( ! psa_is_key_slot_locked( slot ) ) )
            unlocked_persistent_key_slot = slot;
    }

    /*
     * If there is no unused key slot and there is at least one unlocked key
     * slot containing the description of a persistent key, recycle the first
     * such key slot we encountered. If we later need to operate on the
     * persistent key we are evicting now, we will reload its description from
     * storage.
     */
    if( ( selected_slot == NULL ) &&
        ( unlocked_persistent_key_slot != NULL ) )
    {
        selected_slot = unlocked_persistent_key_slot;
        selected_slot->lock_count = 1;
        psa_wipe_key_slot( selected_slot );
    }

    if( selected_slot != NULL )
    {
       status = psa_lock_key_slot( selected_slot );
       if( status != PSA_SUCCESS )
           goto error;

        *volatile_key_id = PSA_KEY_ID_VOLATILE_MIN +
            ( (psa_key_id_t)( selected_slot - global_data.key_slots ) );
        *p_slot = selected_slot;

        return( PSA_SUCCESS );
    }
    status = PSA_ERROR_INSUFFICIENT_MEMORY;

error:
    *p_slot = NULL;
    *volatile_key_id = 0;

    return( status );
}

#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)
static psa_status_t psa_load_persistent_key_into_slot( psa_key_slot_t *slot )
{
    psa_status_t status = PSA_SUCCESS;
    uint8_t *key_data = NULL;
    size_t key_data_length = 0;

    status = psa_load_persistent_key( &slot->attr,
                                      &key_data, &key_data_length );
    if( status != PSA_SUCCESS )
        goto exit;

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    if( psa_key_lifetime_is_external( slot->attr.lifetime ) )
    {
        psa_se_key_data_storage_t *data;
        if( key_data_length != sizeof( *data ) )
        {
            status = PSA_ERROR_STORAGE_FAILURE;
            goto exit;
        }
        data = (psa_se_key_data_storage_t *) key_data;
        memcpy( &slot->data.se.slot_number, &data->slot_number,
                sizeof( slot->data.se.slot_number ) );
    }
    else
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */
    {
        status = psa_copy_key_material_into_slot( slot, key_data, key_data_length );
        if( status != PSA_SUCCESS )
            goto exit;
    }

exit:
    psa_free_persistent_key_data( key_data, key_data_length );
    return( status );
}
#endif /* MBEDTLS_PSA_CRYPTO_STORAGE_C */

psa_status_t psa_get_and_lock_key_slot( mbedtls_svc_key_id_t key,
                                        psa_key_slot_t **p_slot )
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    *p_slot = NULL;
    if( ! global_data.key_slots_initialized )
        return( PSA_ERROR_BAD_STATE );

    /*
     * On success, the pointer to the slot is passed directly to the caller
     * thus no need to unlock the key slot here.
     */
    status = psa_get_and_lock_key_slot_in_memory( key, p_slot );
    if( status != PSA_ERROR_DOES_NOT_EXIST )
        return( status );

#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)
    psa_key_id_t volatile_key_id;

    status = psa_get_empty_key_slot( &volatile_key_id, p_slot );
    if( status != PSA_SUCCESS )
        return( status );

    (*p_slot)->attr.lifetime = PSA_KEY_LIFETIME_PERSISTENT;
    (*p_slot)->attr.id = key;

    status = psa_load_persistent_key_into_slot( *p_slot );
    if( status != PSA_SUCCESS )
        psa_wipe_key_slot( *p_slot );

    return( status );
#else
    return( PSA_ERROR_DOES_NOT_EXIST );
#endif /* defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) */

}

psa_status_t psa_unlock_key_slot( psa_key_slot_t *slot )
{
    if( slot == NULL )
        return( PSA_SUCCESS );

    if( slot->lock_count > 0 )
    {
        slot->lock_count--;
        return( PSA_SUCCESS );
    }

    /*
     * As the return error code may not be handled in case of multiple errors,
     * do our best to report if the lock counter is equal to zero: if
     * available call MBEDTLS_PARAM_FAILED that may terminate execution (if
     * called as part of the execution of a unit test suite this will stop the
     * test suite execution).
     */
#ifdef MBEDTLS_CHECK_PARAMS
    MBEDTLS_PARAM_FAILED( slot->lock_count > 0 );
#endif

    return( PSA_ERROR_CORRUPTION_DETECTED );
}

psa_status_t psa_validate_key_location( psa_key_lifetime_t lifetime,
                                        psa_se_drv_table_entry_t **p_drv )
{
    if ( psa_key_lifetime_is_external( lifetime ) )
    {
#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
        psa_se_drv_table_entry_t *driver = psa_get_se_driver_entry( lifetime );
        if( driver == NULL )
            return( PSA_ERROR_INVALID_ARGUMENT );
        else
        {
            if (p_drv != NULL)
                *p_drv = driver;
            return( PSA_SUCCESS );
        }
#else
        (void) p_drv;
        return( PSA_ERROR_INVALID_ARGUMENT );
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */
    }
    else
        /* Local/internal keys are always valid */
        return( PSA_SUCCESS );
}

psa_status_t psa_validate_key_persistence( psa_key_lifetime_t lifetime )
{
    if ( PSA_KEY_LIFETIME_IS_VOLATILE( lifetime ) )
    {
        /* Volatile keys are always supported */
        return( PSA_SUCCESS );
    }
    else
    {
        /* Persistent keys require storage support */
#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)
        return( PSA_SUCCESS );
#else /* MBEDTLS_PSA_CRYPTO_STORAGE_C */
        return( PSA_ERROR_NOT_SUPPORTED );
#endif /* !MBEDTLS_PSA_CRYPTO_STORAGE_C */
    }
}

psa_status_t psa_open_key( mbedtls_svc_key_id_t key, psa_key_handle_t *handle )
{
#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)
    psa_status_t status;
    psa_key_slot_t *slot;

    status = psa_get_and_lock_key_slot( key, &slot );
    if( status != PSA_SUCCESS )
    {
        *handle = PSA_KEY_HANDLE_INIT;
        return( status );
    }

    *handle = key;

    return( psa_unlock_key_slot( slot ) );

#else /* defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) */
    (void) key;
    *handle = PSA_KEY_HANDLE_INIT;
    return( PSA_ERROR_NOT_SUPPORTED );
#endif /* !defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) */
}

psa_status_t psa_close_key( psa_key_handle_t handle )
{
    psa_status_t status;
    psa_key_slot_t *slot;

    if( psa_key_handle_is_null( handle ) )
        return( PSA_SUCCESS );

    status = psa_get_and_lock_key_slot_in_memory( handle, &slot );
    if( status != PSA_SUCCESS )
        return( status );

    if( slot->lock_count <= 1 )
        return( psa_wipe_key_slot( slot ) );
    else
        return( psa_unlock_key_slot( slot ) );
}

psa_status_t psa_purge_key( mbedtls_svc_key_id_t key )
{
    psa_status_t status;
    psa_key_slot_t *slot;

    status = psa_get_and_lock_key_slot_in_memory( key, &slot );
    if( status != PSA_SUCCESS )
        return( status );

    if( ( ! PSA_KEY_LIFETIME_IS_VOLATILE( slot->attr.lifetime ) ) &&
        ( slot->lock_count <= 1 ) )
        return( psa_wipe_key_slot( slot ) );
    else
        return( psa_unlock_key_slot( slot ) );
}

void mbedtls_psa_get_stats( mbedtls_psa_stats_t *stats )
{
    size_t slot_idx;

    memset( stats, 0, sizeof( *stats ) );

    for( slot_idx = 0; slot_idx < PSA_KEY_SLOT_COUNT; slot_idx++ )
    {
        const psa_key_slot_t *slot = &global_data.key_slots[ slot_idx ];
        if( psa_is_key_slot_locked( slot ) )
        {
            ++stats->locked_slots;
        }
        if( ! psa_is_key_slot_occupied( slot ) )
        {
            ++stats->empty_slots;
            continue;
        }
        if( slot->attr.lifetime == PSA_KEY_LIFETIME_VOLATILE )
            ++stats->volatile_slots;
        else if( slot->attr.lifetime == PSA_KEY_LIFETIME_PERSISTENT )
        {
            psa_key_id_t id = MBEDTLS_SVC_KEY_ID_GET_KEY_ID( slot->attr.id );
            ++stats->persistent_slots;
            if( id > stats->max_open_internal_key_id )
                stats->max_open_internal_key_id = id;
        }
        else
        {
            psa_key_id_t id = MBEDTLS_SVC_KEY_ID_GET_KEY_ID( slot->attr.id );
            ++stats->external_slots;
            if( id > stats->max_open_external_key_id )
                stats->max_open_external_key_id = id;
        }
    }
}

#endif /* MBEDTLS_PSA_CRYPTO_C */
