#include <stdint.h>
#include <stdbool.h>
#include "nrf_error.h"
#include "nrf_drv_rng.h"
#include "app_fifo.h"
#include "mem_manager.h"
#include "mbedtls/ssl.h"
#include "mbedtls/platform.h"
#include "mbedtls/sha256.h"
#include "mbedtls/debug.h"
#include "nrf_tls.h"
#include "app_trace.h"
#include "nrf_assert.h"
#include "iot_timer.h"
#include "iot_errors.h"


/**
 * @defgroup iot_nrf_tls Module's Log Macros
 * @details Macros used for creating module logs which can be useful in understanding handling
 *          of events or actions on API requests.
 * @{
 */
#ifdef  MBEDTLS_DEBUG_C

#define TLS_LOG app_trace_log                                        /**< Used for getting debug information of the module. */
#define TLS_DUMP app_trace_dump                                      /**< Used for getting dump of data when debugging. */

#else //  MBEDTLS_DEBUG_C

#define TLS_LOG(...)                                                 /**< Disables debug information. */
#define TLS_DUMP(...)                                                /**< Disables debug data dump. */

#endif //  MBEDTLS_DEBUG_C

#define TIME_PERIOD_INVALID 0xFFFFFFFF                               /**< Identifier for invalid time period. Used for timer functions. */

#define TLS_MUTEX_LOCK()   SDK_MUTEX_LOCK(m_tls_mutex)               /**< Lock module using mutex */
#define TLS_MUTEX_UNLOCK() SDK_MUTEX_UNLOCK(m_tls_mutex)             /**< Unlock module using mutex */

/**
 * @breif This macro is used for developer debugging.
 *
 * @note  Not enabled nromally as too many logs result and this level of detail is needed
 *        less frequently.
 */
#define TLS_TRC(...)

/**@brief TLS interface. */
typedef struct
{
    uint32_t               transport_id;                             /**< Transport identifier provided by the application to map the TLS instance with associated transport. Set by the application on allocation. */
    nrf_tls_output_t       output_fn;                                /**< Output function registered by the application to write TLS data on the transport. */
    app_fifo_t             input_fifo;                               /**< Input FIFO used for queueing up data received from the transport. Data on the transport us*/
    app_fifo_t             output_fifo;                              /**< Output FIFO used for queueing up decrypted data received on the TLS interface. */
    mbedtls_ssl_context    context;                                  /**< SSL context used by mBedTLS for managing the TLS instance. */
    mbedtls_ssl_config     conf;                                     /**< Pointer to the configuration paramaters used for the instance. Memory is allocated on nrf_tls_alloc. */
#ifdef MBEDTLS_X509_CRT_PARSE_C
    mbedtls_pk_context     pkey;                                     /**< Private key information. Used only when own certificate is provided. */
    mbedtls_x509_crt     * p_owncert;                                /**< Own parsed certificate. */
    mbedtls_x509_crt     * p_cacert;                                 /**< CA's parsed certificate. */
#endif // MBEDTLS_X509_CRT_PARSE_C
    /**@brief Timer management used to provide timeouts to the TLS library. */
    uint32_t               start_tick;                               /**< Indicator (in milliseconds) of when the timeout was requested. */
    uint32_t               intrmediate_delay;                        /**< Period indicating intermediate timeout period in milliseconds. */
    uint32_t               final_delay;                              /**< Final timeout period in milliseconds. */
} interface_t;


#ifdef MBEDTLS_X509_CRT_PARSE_C

/**@breif Input buffer size used for the input FIFO.
 *
 * @note For ECDHE-RSA, though the context length is set to a value smaller than 3072, a buffer size
 *       of 4k is needed for the cloud sends a certificate that does not fit the size.
 *       Also configuration of input FIFO is possible in powers of 2 therefore tuning this value to
 *       a smaller FIFO size to exactly the size of certificate size is not possible.
 */
#define INPUT_BUFFER_SIZE     4096

#endif // MBEDTLS_X509_CRT_PARSE_C

/**@breif Input buffer size when certificates are not used. */
#ifndef INPUT_BUFFER_SIZE
#define INPUT_BUFFER_SIZE MBEDTLS_SSL_MAX_CONTENT_LEN
#endif // INPUT_BUFFER_SIZE

/**@breif Output buffer size.
 *
 * @note Limiting to 1024 instead of MBEDTLS_SSL_MAX_CONTENT_LEN due to RAM constraints.
 */
#define OUTPUT_BUFFER_SIZE  1024


static interface_t * m_interface[NRF_TLS_MAX_INSTANCE_COUNT];                              /**< Interface table to manage the interfaces. */
static uint8_t       m_input_buffer[INPUT_BUFFER_SIZE * NRF_TLS_MAX_INSTANCE_COUNT];       /**< Input buffer that is statically reserved. */
SDK_MUTEX_DEFINE(m_tls_mutex)                                                              /**< Mutex variable. Currently unused, this declaration does not occupy any space in RAM. */

/**@brief Initializes the interface.
 *
 * @param[in] index Identifies instance in m_interface table to be initialized.
 */
static __INLINE void interface_init(uint32_t index)
{
   m_interface[index] = NULL;
}


/**@brief Wrapper function to avoid GCC errors with incompatible parameters.
 *
 * @param[in] n Number of blocks to be allocated.
 * @param[in] s Size of each block to be assigned.
 *
 * @retval Pointer to memory block if procedure succeeded.
 * @retval NULL if procedure failed.
 */
static __INLINE void * wrapper_calloc(size_t n, size_t s)
{
    return nrf_calloc(n,s);
}


/**@brief Frees and allocated interface instance.
 *
 *@param[in] p_instance Identifies the interface instance to be freed.
 */
static void interface_free(uint32_t index)
{
    interface_t * const p_interface = m_interface[index];

    if (p_interface != NULL)
    {
        nrf_free(p_interface->input_fifo.p_buf);
        nrf_free(p_interface->output_fifo.p_buf);
#ifdef MBEDTLS_X509_CRT_PARSE_C
        nrf_free(p_interface->p_cacert);
        nrf_free(p_interface->p_owncert);
#endif // MBEDTLS_X509_CRT_PARSE_C

        mbedtls_ssl_config_free(&p_interface->conf);
        mbedtls_ssl_config_init(&p_interface->conf);

        mbedtls_ssl_free(&p_interface->context);

        nrf_free(p_interface);
    }
    interface_init(index);
}

/**@brief Allocates an interface instance.
 *
 * @param[inout] p_instance Provides transport identifier for the TLS instance.
 *               And if procedure was successful, will conatin allocated TLS instance identifier.
 * @param[in]    TLS options to be used for the instamce.
 *
 * @retval NRF_SUCCESS if the procedure was successful, else an error code indicating reason
 (         for failure.
 */
static uint32_t interface_alloc(nrf_tls_instance_t      * p_instance,
                                nrf_tls_options_t const * p_options)
{
    uint32_t      index       = 0;
    uint32_t      err_code    = NRF_TLS_NO_FREE_INSTANCE;
    interface_t * p_interface = NULL;

    TLS_LOG("[NRF_TLS]: >> interface_alloc\r\n");

    // allocate meory for the TLS instance.
    for (index  = 0; index < NRF_TLS_MAX_INSTANCE_COUNT; index++)
    {
        if (m_interface[index] == NULL)
        {
            err_code = (NRF_ERROR_NO_MEM | IOT_TLS_ERR_BASE);

            // Check if we have space to book keep the instance information.
            p_interface = nrf_malloc(sizeof(interface_t));

            m_interface[index] = p_interface;
            break;
        }
    }

    if ((index < NRF_TLS_MAX_INSTANCE_COUNT)&& (p_interface != NULL))
    {
        // Initialize the memory holding the sturcture.
        memset(p_interface, 0, sizeof(interface_t));

        p_interface->output_fn = p_options->output_fn;

        // Found free instance. Allocate memory for input and output queues.
        uint8_t * p_input_memory  = &m_input_buffer[INPUT_BUFFER_SIZE * index];
        uint8_t * p_output_memory = (uint8_t *)nrf_malloc(OUTPUT_BUFFER_SIZE);

        TLS_LOG("[NRF_TLS]: Input memory address %p\r\n", p_input_memory);
        TLS_LOG("[NRF_TLS]: Output memory address %p\r\n", p_output_memory);

        if (p_output_memory != NULL)
        {
            // Initialize the instance.
            err_code = app_fifo_init(&p_interface->input_fifo,
                                     p_input_memory,
                                     INPUT_BUFFER_SIZE);

            TLS_LOG("[NRF_TLS]: Input FIFO init result %08lx\r\n", err_code);

            if(err_code == NRF_SUCCESS)
            {
                err_code = app_fifo_init(&p_interface->output_fifo,
                                         p_output_memory,
                                         OUTPUT_BUFFER_SIZE);

                TLS_LOG("[NRF_TLS]: Output FIFO init result %08lx\r\n", err_code);

                if (err_code == NRF_SUCCESS)
                {
                    // All pre-requisites for using the instance met.
                    // Procedure was successful.
                    TLS_LOG("[NRF_TLS]: Ind\r\n");

                    p_interface->transport_id = p_instance->transport_id;
                    p_instance->instance_id   = index;
                }
            }

            if (err_code != NRF_SUCCESS)
            {
                err_code = (NRF_ERROR_INTERNAL | IOT_TLS_ERR_BASE);
            }
        }

        if (err_code != NRF_SUCCESS)
        {
            TLS_LOG("[NRF_TLS]: Allocation failed - Initialization procedures.\r\n");
            interface_free(index);
        }
    }
    else
    {
        TLS_LOG("[NRF_TLS]: >> Allocation failed - Inadequate memory.\r\n");
    }

    return err_code;
}


/**@brief Transport read function registered with the TLS library.
 *
 * @details Data read on the transport is fed to the interface using the nrf_tls_input function.
 *          The TLS library requests the data based on state of SSL connection.
 *
 * @param[in]  p_ctx     Context registered with the library on creation of the TLS instance.
 * @param[out] p_buffer  Buffer where read data is fetched.
 * @param[in]  size      Size to be read.
 *
 * @retval size of data read if procedure was successful.
 * @retval MBEDTLS_ERR_SSL_CONN_EOF indicating there is not enough data received on the transport.
 */
static int interface_transport_read(void * p_ctx, unsigned char * p_buffer, size_t buffer_size)
{
    uint32_t            err;
    uint32_t            length         = buffer_size;
    interface_t * const p_interface    = m_interface[(uint32_t)p_ctx];
    uint32_t            available_size = 0;

    TLS_MUTEX_LOCK();

    TLS_TRC("[NRF-TLS]:[%p]: interface_transport_read requested %08x\r\n",
             p_interface,
             buffer_size);

    // Verify how much data is available in the queue.
    err = app_fifo_read(&p_interface->input_fifo, NULL, &available_size);

    TLS_TRC("[NRF-TLS]:[%p]: interface_transport_read, app_fifo_read result %ld\r\n",
             p_interface,
             err);

    // For datagram connection, read all that is available.
    // For stream sockets, read only if available data is at least as much as requested.
    if  (( (p_interface->conf.transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) &&
           (available_size > 0)                                               )||
         ( (p_interface->conf.transport == MBEDTLS_SSL_TRANSPORT_STREAM)   &&
           (available_size >= buffer_size))                                   )
    {
        TLS_TRC("[NRF-TLS]:[%p]: interface_transport_read requested 0x%08x, available %08lx.\r\n",
                 p_interface,
                 buffer_size,
                 available_size);

        err = app_fifo_read(&p_interface->input_fifo, p_buffer, &length);

        if (err == NRF_SUCCESS)
        {
            TLS_TRC("[NRF-TLS]:[%p]: interface_transport_read success, length 0x%08lx.\r\n",
                     p_interface,
                     length);

            TLS_TRC("\r\n\r\n[NRF TLS]: ---------------- SSL Read data --------------\r\n");
            TLS_DUMP(p_buffer, length);
            TLS_TRC("\r\n[NRF TLS]: -------------------- End ------------------\r\n\r\n");

            return length;
        }
    }
    else
    {
        TLS_TRC("[NRF-TLS]:[%p]: interface_transport_read requested size 0x%08x, "
                "available 0x%08lx.\r\n",
                p_interface,
                buffer_size,
                available_size);
    }

    TLS_MUTEX_UNLOCK();

    return MBEDTLS_ERR_SSL_CONN_EOF;
}


/**@brief Write function that the TLS library calls to write on the transport.
 *
 * @param[in] p_ctx  Context registered with the library on creation of the TLS instance.
 * @param[in] p_buf  Buffer containing data to be written on the transport.
 * @param[in] len    Length of data to be written.
 *
 * @retval length of data written on the transport if the procedure was successful.
 * @retval MBEDTLS_ERR_SSL_CONN_EOF in case the procedure failed.
 */
static int interface_transport_write(void * p_ctx, const unsigned char * p_buf, size_t len)
{
    int                      op_len      = len;
    interface_t * const      p_interface = m_interface[(uint32_t)p_ctx];

    TLS_MUTEX_LOCK();

    const nrf_tls_instance_t handle = {
        .transport_id = p_interface->transport_id,
        .instance_id  = (uint32_t)p_ctx
    };

    TLS_LOG("[NRF-TLS]:[%p]: interface_transport_write requested 0x%08x\r\n",
               p_interface, len);

    TLS_MUTEX_UNLOCK();

    uint32_t err_code = p_interface->output_fn(&handle, (uint8_t *)p_buf, len);

    TLS_MUTEX_LOCK();

    if (err_code != NRF_SUCCESS)
    {
        op_len = MBEDTLS_ERR_SSL_CONN_EOF;
    }

    TLS_MUTEX_UNLOCK();

    return op_len;
}


/**
 * @brief Random number generator registered with the TLS library to create random numbers
 *        when needed.
 *
 * @param[in]  p_ctx    Context registered with the library on creation of the TLS instance.
 * @param[out] p_buffer Buffer where generated random vector is to be fetched.
 * @param[in]  size     Requested size of the random vector.
 *
 * @retval 0 if procedure was successful.
 */
static int random_vector_generate(void * p_ctx, unsigned char * p_buffer, size_t size)
{
    uint8_t available;
    uint32_t err_code;

    err_code = nrf_drv_rng_bytes_available(&available);

    TLS_TRC("[NRF-TLS]: >> random_vector_generate, requested 0x%08lx, available 0x%08x\r\n",
            size,
            available);

    if (err_code == NRF_SUCCESS)
    {
        uint16_t length = MIN(size , available);

        err_code = nrf_drv_rng_rand(p_buffer, length);
    }

    return 0;
}


/**@breif Routine called periodically to adavnce the SSL context state.
 *
 * @breif param[in] index Identifies the instance in m_interface to be serviced.
 */
static void interface_continue(interface_t * p_interface)
{
    uint8_t * data = nrf_malloc(OUTPUT_BUFFER_SIZE);

    if (data != NULL)
    {
        int len;

        TLS_MUTEX_UNLOCK();

        len = mbedtls_ssl_read(&p_interface->context, &data[0], OUTPUT_BUFFER_SIZE);

        TLS_MUTEX_LOCK();

        TLS_TRC("[NRF-TLS]:[%p]:[0x%08lx]mbedtls_ssl_read result(len) 0x%08lx\r\n",
                p_interface,
                index,
                len);

        if (len > 0)
        {
            uint32_t write_len = len;

            uint32_t err_code = app_fifo_write(&p_interface->output_fifo, data, &write_len);
            if (err_code != NRF_SUCCESS)
            {
                TLS_LOG("[NRF-TLS]: Failed to write decrypted data.\r\n");
            }
        }

        nrf_free(data);
    }
}

/**@brief Debug log funciton registered with the TLS library.
 *
 * @param[in]  p_ctx    Context registered with the library on creation of the TLS instance.
 * @param[in]  level    Debug level of the log.
 * @param[in]  p_file   File requesting the log.
 * @param[in]  line     Line number in the file requesting the log.
 * @param[in]  p_str    String containing the log message.
 */
static void mbedtls_log(void       * p_ctx,
                     int          level,
                     const char * p_file,
                     int          line,
                     const char * p_str)
{
    TLS_LOG("[--TLS--]:[%s]:[%d]: %s\r\n", &p_file[strlen(p_file)-12], line, p_str);
}


/**@brief Function registered with TLS library to set (start/stop) timer.
 *
 * @param[in]  p_ctx    Context registered with the library on creation of the TLS instance.
 * @param[in]  int_ms   Intermediate timeout period in milliseconds.
 * @param[in]  fin_ms   Finaly timout period in milliseconds.
 *                      Value of zero indicates a request to stop the timer.
 */
static void tls_set_timer (void * p_ctx, uint32_t int_ms, uint32_t fin_ms)
{
    TLS_MUTEX_LOCK();

    interface_t * const p_interface = m_interface[(uint32_t)p_ctx];

    if (fin_ms > 0)
    {
        TLS_TRC("[NRF-TLS]:[%p]: set_timer, start %d, final %d, intermediate %d\r\n",
                p_interface,
                p_interface->start_tick,
                p_interface->final_delay,
                p_interface->intrmediate_delay);

        uint32_t err_code = iot_timer_wall_clock_get(&p_interface->start_tick);

        if (err_code == NRF_SUCCESS)
        {
            p_interface->final_delay       = fin_ms;
            p_interface->intrmediate_delay = int_ms;
        }
    }
    else
    {
        p_interface->start_tick        = TIME_PERIOD_INVALID;
        p_interface->final_delay       = TIME_PERIOD_INVALID;
        p_interface->intrmediate_delay = 0;
    }

    TLS_MUTEX_UNLOCK();
}


/**@breif Function registered with TLS library to get the status of the timer.
 *
 * @param[in]  p_ctx    Context registered with the library on creation of the TLS instance.
 *
 * @retval -1 if timer is cancelled
 * @retval  0 if none of the delays is expired
 * @retval  1 if the intermediate delay only is expired
 * @retval  2 if the final delay is expired
 */
static int tls_get_timer (void * p_ctx)
{
    TLS_MUTEX_LOCK();

    iot_timer_time_in_ms_t         elapsed_time;
    interface_t            * const p_interface = m_interface[(uint32_t)p_ctx];

    uint32_t err_code = iot_timer_wall_clock_delta_get(&p_interface->start_tick, &elapsed_time);

    TLS_TRC("[NRF-TLS]:[%p]:get_timer, start %d, final %d, intermediate %d, elapsed %d\r\n",
            p_interface,
            p_interface->start_tick,
            p_interface->final_delay,
            p_interface->intrmediate_delay,
            elapsed_time);

    if (err_code == NRF_SUCCESS)
    {
        if (p_interface->final_delay <= elapsed_time)
        {
            return 2;
        }
        else if (p_interface->intrmediate_delay <= elapsed_time)
        {
           return 1;
        }
        else
        {
            if ((p_interface->intrmediate_delay ==  TIME_PERIOD_INVALID) &&
                (p_interface->final_delay       ==  TIME_PERIOD_INVALID))
            {
                return -1;
            }
        }
    }

    TLS_MUTEX_UNLOCK();

    return 0;
}


/**@brief Prints/displays the configuration of the interface. Used for debugging purposes.
 *
 * @param[in] p_interface Identifies the instance whose ssl configuration is to be printed.
 */
static void interface_conf_debug_print(interface_t * p_interface)
{

    TLS_TRC("[NRF-TLS]:[%p] SSL Conf\r\n", p_interface);

    for (int i = 0; i < 4; i++)
    {
        TLS_TRC("\t Cipger Suites : %02d\r\n", i);
        TLS_TRC("\t\t\tCiphersuite[0] = %02d\r\n", p_interface->conf.ciphersuite_list[i][0]);
        TLS_TRC("\t\t\tCiphersuite[1] = %02d\r\n", p_interface->conf.ciphersuite_list[i][1]);
        TLS_TRC("\t\t\tCiphersuite[2] = %02d\r\n", p_interface->conf.ciphersuite_list[i][2]);
        TLS_TRC("\t\t\tCiphersuite[3] = %02d\r\n", p_interface->conf.ciphersuite_list[i][3]);
    }

    TLS_TRC("\t\tf_dbg           = %p\r\n", p_interface->conf.f_dbg);
    TLS_TRC("\t\tp_dbg           = %p\r\n", p_interface->conf.p_dbg);

    TLS_TRC("\t\tf_rng           = %p\r\n", p_interface->conf.f_rng);
    TLS_TRC("\t\tp_rng           = %p\r\n", p_interface->conf.p_rng);

    TLS_TRC("\t\tf_get_cache     = %p\r\n", p_interface->conf.f_get_cache);
    TLS_TRC("\t\tf_set_cache     = %p\r\n", p_interface->conf.f_set_cache);
    TLS_TRC("\t\tf_set_cache     = %p\r\n", p_interface->conf.p_cache);

    TLS_TRC("\t\tf_vrfy          = %p\r\n", p_interface->conf.f_vrfy);
    TLS_TRC("\t\tp_vrfy          = %p\r\n", p_interface->conf.p_vrfy);

    TLS_TRC("\t\tcert_profile    = %p\r\n", p_interface->conf.cert_profile);
    TLS_TRC("\t\tkey_cert        = %p\r\n", p_interface->conf.key_cert);
    TLS_TRC("\t\tca_chain        = %p\r\n", p_interface->conf.ca_chain);
    TLS_TRC("\t\tca_crl          = %p\r\n", p_interface->conf.ca_crl);

    TLS_TRC("\t\tmax_major_ver   = %2d\r\n", p_interface->conf.max_major_ver);
    TLS_TRC("\t\tmax_minor_ver   = %2d\r\n", p_interface->conf.max_minor_ver);
    TLS_TRC("\t\tmin_major_ver   = %2d\r\n", p_interface->conf.min_major_ver);
    TLS_TRC("\t\tmax_major_ver   = %2d\r\n", p_interface->conf.max_major_ver);
}


/**@breif Sets up own certificate on the interface instance.
 *
 * @param[in] p_interface TLS interface instance for which the procedure is requested.
 * @param[in] p_own       Supplies own certifcate setup information.
 *
 * @retval NRF_SUCCESS if the procedure was successful, else an error code indicating reason for
 *         failure.
 */
static uint32_t own_certificate_set(interface_t                 * const p_interface,
                                              nrf_tls_certificate_t const * p_own)
{
#ifdef MBEDTLS_X509_CRT_PARSE_C
    uint32_t error_code = (NRF_ERROR_NO_MEM | IOT_TLS_ERR_BASE);

    int result;

    p_interface->p_owncert = nrf_malloc(sizeof(mbedtls_x509_crt));

    if (p_interface->p_owncert)
    {
        mbedtls_x509_crt_init(p_interface->p_owncert);

        result = mbedtls_x509_crt_parse(p_interface->p_owncert,
                                        p_own->p_certificate,
                                        p_own->certificate_len);

        if( result == 0 )
        {
            mbedtls_pk_init( &p_interface->pkey );

            result =  mbedtls_pk_parse_key(&p_interface->pkey,
                                           p_own->p_private_key,
                                           p_own->private_key_len,
                                           NULL,
                                           0 );

            mbedtls_ssl_conf_own_cert(&p_interface->conf,
                                      p_interface->p_owncert,
                                      &p_interface->pkey);
        }
        else
        {
            error_code = NRF_TLS_OWN_CERT_SETUP_FAILED;
        }
    }

    return result;
#else
    return NRF_SUCCESS;
#endif // MBEDTLS_X509_CRT_PARSE_C
}


/**@breif Sets up verification option for the interfce,
 *
 * @param[in] p_interface TLS interface instance for which the procedure is requested.
 * @param[in] p_settings  Key setting supplying CA PEM file.
 *
 * @retval NRF_SUCCESS if the procedure was successful, else an error code indicating reason for
 *         failure.
 */
static uint32_t verify_options_set(interface_t                  * const  p_interface,
                                   nrf_tls_key_settings_t const * p_settings)
{
    uint32_t err_code = NRF_SUCCESS;

#ifdef MBEDTLS_X509_CRT_PARSE_C
    if ((p_settings != NULL) && (p_settings->p_ca_cert_pem != NULL))
    {
        mbedtls_ssl_conf_authmode(&p_interface->conf, MBEDTLS_SSL_VERIFY_REQUIRED );

        p_interface->p_cacert = nrf_malloc(sizeof(mbedtls_x509_crt));

        if (p_interface->p_cacert != NULL)
        {
            // Initialize ca certificate.
            mbedtls_x509_crt_init(p_interface->p_cacert);

            int result = mbedtls_x509_crt_parse(p_interface->p_cacert,
                                                p_settings->p_ca_cert_pem,
                                                sizeof(p_settings->p_ca_cert_pem));
            if( result < 0 )
            {
                err_code = NRF_TLS_INVALID_CA_CERTIFICATE;
            }
            else
            {
                mbedtls_ssl_conf_ca_chain(&p_interface->conf, p_interface->p_cacert, NULL);
            }
        }
        else
        {
            err_code = (NRF_ERROR_NO_MEM | IOT_TLS_ERR_BASE);
        }
    }
    else
#endif // MBEDTLS_X509_CRT_PARSE_C
    {
        mbedtls_ssl_conf_authmode(&p_interface->conf, MBEDTLS_SSL_VERIFY_OPTIONAL );
    }

    return err_code;
}


/**@breif Sets up the configuration for SSL context according to the options specificed.
 *
 * @param[in] instance_id Identifies the TLS instance for which the procedure is requested.
 * @param[in] p_options   Supplies options to be used for the configuration.
 *
 * @retval NRF_SUCCESS if the procedure was successful, else an error indicating reason
 *         for failure.
 */
static uint32_t interface_conf_setup(uint32_t instance_id, nrf_tls_options_t const * p_options)
{
    int           result      = 0;
    interface_t * p_interface = m_interface[instance_id];
    uint32_t      err_code    = NRF_TLS_CONFIGURATION_FAILED;

    mbedtls_ssl_config_init(&p_interface->conf);

    result = mbedtls_ssl_config_defaults(&p_interface->conf,
                                         p_options->role,
                                         p_options->transport_type,
                                         MBEDTLS_SSL_PRESET_DEFAULT);

    mbedtls_ssl_conf_rng(&p_interface->conf, random_vector_generate, NULL);
    mbedtls_ssl_conf_dbg(&p_interface->conf, mbedtls_log, NULL);

    TLS_TRC("[NRF-TLS]:[%p]: mbedtls_ssl_config_defaults result %08lx\r\n", p_conf, result);

#ifdef MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
    if (result == 0)
    {
        if ((p_options->p_key_settings != NULL) && (p_options->p_key_settings->p_psk != NULL))
        {
            result = mbedtls_ssl_conf_psk(&p_interface->conf,
                                          p_options->p_key_settings->p_psk->p_secret_key,
                                          p_options->p_key_settings->p_psk->secret_key_len,
                                          p_options->p_key_settings->p_psk->p_identity,
                                          p_options->p_key_settings->p_psk->identity_len);
        }
    }
#endif // MBEDTLS_KEY_EXCHANGE_PSK_ENABLED

    if (result == 0)
    {
        if (p_options->p_key_settings->p_own_certificate != NULL)
        {
            err_code = own_certificate_set(p_interface,
                                           p_options->p_key_settings->p_own_certificate);
        }
        else
        {
            err_code = NRF_SUCCESS;
        }
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = verify_options_set(p_interface, p_options->p_key_settings);
    }

#ifdef MBEDTLS_SSL_PROTO_DTLS
    if (err_code == NRF_SUCCESS)
    {
        if (p_interface->conf.transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM)
        {
            mbedtls_ssl_conf_min_version(&p_interface->conf,
                                         MBEDTLS_SSL_MAJOR_VERSION_3,
                                         MBEDTLS_SSL_MINOR_VERSION_3);

            mbedtls_ssl_conf_max_version(&p_interface->conf,
                                         MBEDTLS_SSL_MAJOR_VERSION_3,
                                         MBEDTLS_SSL_MINOR_VERSION_3);

            if (p_options->transport_type == MBEDTLS_SSL_TRANSPORT_DATAGRAM)
            {
                mbedtls_ssl_conf_handshake_timeout(&p_interface->conf,
                                                   (MBEDTLS_SSL_DTLS_TIMEOUT_DFL_MIN * 2),
                                                   (MBEDTLS_SSL_DTLS_TIMEOUT_DFL_MIN * 2));
            }
        }
    }
#endif // MBEDTLS_SSL_PROTO_DTLS

    interface_conf_debug_print(p_interface);

    return err_code;
}


/**@breif Sets up the SSL context according to the options specificed.
 *
 * @param[in] instance_id Identifies the TLS instance for which the procedure is requested.
 *
 * @retval NRF_SUCCESS if the procedure was successful, else an error indicating reason for
 *         failure.
 *
 * @note For client roles, handshake prcoedure is initiated here.
 */
static uint32_t interface_ssl_context_setup(uint32_t instance_id)
{
    int           result = 0;
    interface_t * p_interface = m_interface[instance_id];

    TLS_TRC("[NRF-TLS]:[%p]: Major number: 0x%08lx Minor number: 0x%08lx\r\n",
            p_conf,
            p_conf->min_major_ver,
            p_conf->min_minor_ver);

    mbedtls_ssl_init(&p_interface->context);

    result = mbedtls_ssl_setup(&p_interface->context, &p_interface->conf);

    TLS_LOG("[NRF-TLS]: mbedtls_ssl_setup result %d\r\n",result);

    if (result == 0)
    {
        //ssl_set_ciphersuites(&p_interface->context, default_ciphers);
        mbedtls_ssl_set_bio(&p_interface->context,
                            (void *)instance_id,
                            interface_transport_write,
                            interface_transport_read,
                            NULL);

        if (p_interface->conf.transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM)
        {
             mbedtls_ssl_set_timer_cb(&p_interface->context,
                                      (void *)instance_id,
                                      tls_set_timer,
                                      tls_get_timer);
        }

        TLS_MUTEX_UNLOCK();

        result = mbedtls_ssl_handshake(&p_interface->context);

        TLS_MUTEX_LOCK();

        TLS_LOG("[NRF-TLS]: mbedtls_ssl_handshake result %d\r\n", result);

        if (result == MBEDTLS_ERR_SSL_CONN_EOF)
        {
            result = 0;
        }
    }

    return (result ? (NRF_ERROR_INTERNAL | IOT_TLS_ERR_BASE) : NRF_SUCCESS);
}


uint32_t nrf_tls_init(void)
{
    uint32_t           index = 0;

    SDK_MUTEX_INIT(m_tls_mutex);

    do
    {
        interface_init(index);
        index++;
    } while(index < NRF_TLS_MAX_INSTANCE_COUNT);

    mbedtls_platform_set_calloc_free(wrapper_calloc, nrf_free);

#ifdef MBEDTLS_DEBUG_C
    mbedtls_debug_set_threshold(2);
#endif // MBEDTLS_DEBUG_C

    nrf_drv_rng_init(NULL);

    return NRF_SUCCESS;
}


uint32_t nrf_tls_alloc(nrf_tls_instance_t      * p_instance,
                       nrf_tls_options_t const * p_options)
{
    uint32_t err_code = (NRF_ERROR_NO_MEM | IOT_TLS_ERR_BASE);

    TLS_MUTEX_LOCK();

    err_code = interface_alloc(p_instance, p_options);

    if (err_code == NRF_SUCCESS)
    {
        err_code = interface_conf_setup(p_instance->instance_id, p_options);
    }

    if (err_code == NRF_SUCCESS)
    {
        err_code = interface_ssl_context_setup(p_instance->instance_id);
    }

    if (err_code != NRF_SUCCESS)
    {
        interface_free(p_instance->instance_id);
    }

    TLS_MUTEX_UNLOCK();

    return err_code;
}


uint32_t nrf_tls_input(nrf_tls_instance_t const * p_instance,
                       uint8_t            const * p_data,
                       uint32_t                   datalen)
{

    uint32_t actual_size = 0;
    uint32_t err_code    = (NRF_ERROR_NOT_FOUND | IOT_TLS_ERR_BASE);

    TLS_MUTEX_LOCK();

    if ((p_instance->instance_id < NRF_TLS_MAX_INSTANCE_COUNT) &&
        (m_interface[p_instance->instance_id] != NULL))
    {
        interface_t * const p_interface = m_interface[p_instance->instance_id];

        // Verify all data can be queued in the FIFO.
        err_code = app_fifo_write(&p_interface->input_fifo, NULL, &actual_size);
        if (err_code == NRF_SUCCESS)
        {
            // Avoid partial write.
            if (datalen <= actual_size)
            {
                actual_size = datalen;

                err_code = app_fifo_write(&p_interface->input_fifo, p_data, &actual_size);

                TLS_TRC("[NRF-TLS]:[%p]: >>  nrf_tls_input datalen 0x%08lx result 0x%08lx\r\n",
                        p_interface,
                        datalen,
                        retval);
            }
            else
            {
                TLS_LOG("[NRF-TLS]:[%p]: Failed to queue input. "
                            "Available 0x%08lx, requested 0x%08lx\r\n",
                            p_interface,
                            actual_size,
                            datalen);
            }

            interface_continue(p_interface);
        }
        else
        {
            // Not enough room in the FIFO, indicate error.
            return (NRF_ERROR_NO_MEM | IOT_TLS_ERR_BASE);
        }
    }

    TLS_MUTEX_UNLOCK();

    return err_code;
}


uint32_t nrf_tls_read(nrf_tls_instance_t const * p_instance,
                      uint8_t                  * p_data,
                      uint32_t                 * p_datalen)
{
    uint32_t err_code = (NRF_ERROR_NOT_FOUND | IOT_TLS_ERR_BASE);

    TLS_MUTEX_LOCK();

    if ((p_instance->instance_id < NRF_TLS_MAX_INSTANCE_COUNT) &&
        (m_interface[p_instance->instance_id] != NULL))
    {
        interface_t * const p_interface = m_interface[p_instance->instance_id];

        err_code = app_fifo_read(&p_interface->output_fifo, p_data, p_datalen);
    }

    TLS_MUTEX_UNLOCK();

    return err_code;
}


uint32_t nrf_tls_write(nrf_tls_instance_t const * p_instance,
                       uint8_t            const * p_data,
                       uint32_t                 * p_datalen)
{
    uint32_t err_code = (NRF_ERROR_NOT_FOUND | IOT_TLS_ERR_BASE);

    TLS_MUTEX_LOCK();

    if ((p_instance->instance_id < NRF_TLS_MAX_INSTANCE_COUNT) &&
        (m_interface[p_instance->instance_id] != NULL))
    {
        interface_t * const p_interface = m_interface[p_instance->instance_id];

        int actual_len = (*p_datalen);

        TLS_MUTEX_UNLOCK();

        actual_len = mbedtls_ssl_write(&p_interface->context, p_data, actual_len);

        TLS_MUTEX_LOCK();

        if (actual_len < 0)
        {
            err_code = (NRF_ERROR_INTERNAL | IOT_TLS_ERR_BASE);

            if (actual_len == MBEDTLS_ERR_SSL_CONN_EOF)
            {
                if (p_interface->context.state < MBEDTLS_SSL_HANDSHAKE_OVER)
                {
                    err_code = NRF_TLS_HANDSHAKE_IN_PROGRESS;
                }
            }
        }
        else
        {
            (* p_datalen) = actual_len;
            err_code      = NRF_SUCCESS;
        }
    }

    TLS_MUTEX_UNLOCK();

    return err_code;
}


uint32_t nrf_tls_free(nrf_tls_instance_t const * p_instance)
{
    uint32_t err_code = (NRF_ERROR_NOT_FOUND | IOT_TLS_ERR_BASE);

    TLS_MUTEX_LOCK();

    if ((p_instance->instance_id < NRF_TLS_MAX_INSTANCE_COUNT) &&
        (m_interface[p_instance->instance_id] != NULL))
    {
        interface_free(p_instance->instance_id);
        err_code = NRF_SUCCESS;
    }

    TLS_MUTEX_UNLOCK();

    return err_code;
}


void nrf_tls_process(void)
{
    uint32_t      index;

    TLS_MUTEX_LOCK();

    for (index = 0; index < NRF_TLS_MAX_INSTANCE_COUNT; index++)
    {
        if (m_interface[index] != NULL)
        {
            interface_continue(m_interface[index]);
        }
    }

    TLS_MUTEX_UNLOCK();
}
