#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <uavcan/uavcan.hpp>
#include <uavcan_stm32/uavcan_stm32.hpp>
#include <platform-abstraction/threading.h>
#include <platform-abstraction/panic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include <uavcan/cvra/CVRAService.hpp>
#include <uavcan/cvra/CVRAService2.hpp>
#include <uavcan/cvra/CVRAService3.hpp>
#include <platform-abstraction/timestamp.h>
#include <src/ins-board.h>

#include <stdarg.h>

#define CAN_BITRATE 1000000

#define NODE_SERVER 1

#define SERVER_NODE_ID 42
#define CLIENT_NODE_ID 23

#if NODE_SERVER
# define NODE_ID SERVER_NODE_ID
# define NODE_NAME "server"
#else
# define NODE_ID CLIENT_NODE_ID
# define NODE_NAME "client"
#endif

uavcan_stm32::CanInitHelper<128> can;

typedef uavcan::Node<16384> Node;

uavcan::LazyConstructor<Node> node_;


Node& getNode()
{
    if (!node_.isConstructed()) {
        node_.construct<uavcan::ICanDriver&, uavcan::ISystemClock&>(can.driver, uavcan_stm32::SystemClock::instance());
    }
    return *node_;
}

void can2_gpio_init(void)
{
    // enable CAN transceiver
    gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
    gpio_clear(GPIOC, GPIO5);
    gpio_set_af(GPIOB, GPIO_AF9, GPIO12 | GPIO13);
    gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO12 | GPIO13);
}

#if NODE_SERVER

void trace_request(void)
{
    static unsigned int request_counter = 0;
    static uint32_t start = 0;

    if (request_counter++ == 0) {
        start = os_timestamp_get();
    } else if (os_timestamp_get() - start > 1000000) {
        printf("req/sec: %u\n",  request_counter);
        request_counter = 0;
    }
    STATUS_LED_TOGGLE();
}

void cvra_server_cb(const uavcan::ReceivedDataStructure<uavcan::cvra::CVRAService::Request>& req,
                    uavcan::cvra::CVRAService::Response& rsp)
{
    trace_request();
    const uint32_t d = req.request_data;
    rsp.response_data = d;
}

void cvra_server_cb2(const uavcan::ReceivedDataStructure<uavcan::cvra::CVRAService2::Request>& req,
                    uavcan::cvra::CVRAService2::Response& rsp)
{
    trace_request();
    const uint32_t d = req.request_data;
    rsp.response_data = d;
}

void cvra_server_cb3(const uavcan::ReceivedDataStructure<uavcan::cvra::CVRAService3::Request>& req,
                    uavcan::cvra::CVRAService3::Response& rsp)
{
    trace_request();
    const uint32_t d = req.request_data;
    rsp.response_data = d;
}
#else

uint32_t trace(bool start)
{
    uint32_t diff = 0;
    static int print = 100;
    static uint32_t start_ts;
    if (start) {
        start_ts = os_timestamp_get();
    } else {
        diff = os_timestamp_get() - start_ts;
        if (print-- == 0) {
            printf("trace: %u usec\n", (unsigned int) diff);
            print = 100;
        }
    }
    return diff;
}

void cvra_client_cb(const uavcan::ServiceCallResult<uavcan::cvra::CVRAService>& call_result)
{
    // trace(false);
    const uint32_t resp = call_result.response.response_data;
    if (resp == 42) {
        STATUS_LED_TOGGLE();
    } else {
        ERROR_LED_TOGGLE();
    }
}

void cvra_client_cb2(const uavcan::ServiceCallResult<uavcan::cvra::CVRAService2>& call_result)
{

}

void cvra_client_cb3(const uavcan::ServiceCallResult<uavcan::cvra::CVRAService3>& call_result)
{

}
#endif

void cpp_node_main(void)
{
    std::printf("Node Thread main\n");

    can2_gpio_init();
    std::printf("can lowlevel init\n");

    int init = can.init(CAN_BITRATE);

    if (init != 0) {
        std::printf("can driver init error\n");
        while(1);
    }
    std::printf("can driver init\n");

    /*
     * Setting up the node parameters
     */
    Node& node = getNode();

    node.setNodeID(NODE_ID);
    node.setName(NODE_NAME);

    std::printf("Node init %u\n", node.getNodeID().get());

    /*
     * Initializing the UAVCAN node - this may take a while
     */
    while (true) {
        int res = node.start();

        if (res < 0) {
            std::printf("Node initialization failure: %i, will try agin soon\n", res);
        } else {
            break;
        }
        os_thread_sleep_us(1000);
    }

#if NODE_SERVER
    /* CVRAService server */
    std::printf("config CVRAService server\n");

    uavcan::ServiceServer<uavcan::cvra::CVRAService> srv(node);
    const int srv_start_res = srv.start(cvra_server_cb);
    if (srv_start_res < 0) {
        std::printf("failed to start CVRAService server\n");
        while(1) {
            os_thread_sleep_us(1000);
        }
    }
    uavcan::ServiceServer<uavcan::cvra::CVRAService2> srv2(node);
    srv2.start(cvra_server_cb2);
    uavcan::ServiceServer<uavcan::cvra::CVRAService3> srv3(node);
    srv3.start(cvra_server_cb3);
#else
    /* CVRAService client */
    std::printf("config CVRAService client\n");

    uavcan::ServiceClient<uavcan::cvra::CVRAService> client(node);
    const int client_init_res = client.init();
    if (client_init_res < 0) {
        std::printf("failed to start CVRAService client\n");
        while(1) {
            os_thread_sleep_us(1000);
        }
    }

    client.setCallback(cvra_client_cb);

    uavcan::ServiceClient<uavcan::cvra::CVRAService2> client2(node);
    client2.init();
    client2.setCallback(cvra_client_cb2);

    uavcan::ServiceClient<uavcan::cvra::CVRAService3> client3(node);
    client3.init();
    client3.setCallback(cvra_client_cb3);
#endif

    /*
     * Informing other nodes that we're ready to work.
     * Default status is INITIALIZING.
     */
    node.setStatusOk();

    /*
     * Main loop
     */
    std::printf("UAVCAN node started\n");

    while (true) {
#if NODE_SERVER
        int spin_res = node.spin(uavcan::MonotonicDuration::fromMSec(1000));
        if (spin_res < 0) {
            std::printf("Spin failure: %i\n", spin_res);
        }
#else
        if (!client.isPending()) {
            uavcan::cvra::CVRAService::Request req;
            req.request_data = 42;

            const int res = client.call(SERVER_NODE_ID, req);

            if (res < 0) {
                printf("CVRAService request call failed\n");
            }
            // trace(true);
        }

        if (!client2.isPending()) {
            uavcan::cvra::CVRAService2::Request req;
            req.request_data = 42;
            const int res = client2.call(SERVER_NODE_ID, req);
            if (res < 0) {
                printf("CVRAService request call failed\n");
            }
        }

        if (!client3.isPending()) {
            uavcan::cvra::CVRAService3::Request req;
            req.request_data = 42;
            const int res = client3.call(SERVER_NODE_ID, req);
            if (res < 0) {
                printf("CVRAService request call failed\n");
            }
        }

        int spin_res = node.spin(uavcan::MonotonicDuration::fromMSec(0));
        if (spin_res < 0) {
            std::printf("Spin failure: %i\n", spin_res);
        }
#endif

        gpio_toggle(GPIOA, GPIO8);
    }
}

/*
int uavcan_stm32_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    printf("stm32_can: ");
    vprintf(fmt, ap);
    printf("\n");

    va_end(ap);
}
*/

/*
void uavcan_assert(const char *file, int line, bool cond)
{
    if (!cond) {
        std::printf("%s:%d: assert failed\n", file, line);
        while(1);
    }
}
*/

extern "C"
void node_main(void *arg)
{
    (void) arg;
    cpp_node_main();
}
