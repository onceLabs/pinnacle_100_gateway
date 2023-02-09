
#include <zephyr/kernel.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/modem/hl7800.h>

#include "p100_socket.h"

LOG_MODULE_REGISTER(p100_socket_log, LOG_LEVEL_INF);

// Thread stack setup
#define P100_SOCKET_STACK_SIZE 1024
#define P100_SOCKET_PRIORITY 5

static void p100_socket_thread();

K_THREAD_STACK_DEFINE(p100_socket_stack_area, P100_SOCKET_STACK_SIZE);
struct k_thread p100_socket_thread_data;

// Hostname and port for the echo server
#define SERVER_IP "20.56.165.163"
#define SERVER_PORT_INT 2444

#define MESSAGE_SIZE 256
#define MESSAGE_TO_SEND "Hello from Pinnacle 100"
#define SSTRLEN(s) (sizeof(s) - 1)

// Structure for the socket and server address
static int sock;
static struct sockaddr_storage server;

// Buffer for receiving from server
static uint8_t recv_buf[MESSAGE_SIZE];

static int server_resolve(void)
{
    // Retrieve the relevant information from the result structure
    struct sockaddr_in *server4 = ((struct sockaddr_in *)&server);

    inet_pton(AF_INET, SERVER_IP, &server4->sin_addr.s_addr);
    server4->sin_family = AF_INET;

    // Flip the port bytes because of how it is being cast
    // This is an error in Zephyr
    server4->sin_port = ((SERVER_PORT_INT >> 8) & 0xff) | ((SERVER_PORT_INT << 8) & 0xff00);

    // Convert the address into a string and print it
    char ipv4_addr[NET_IPV4_ADDR_LEN];
    inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr,
              sizeof(ipv4_addr));
    LOG_INF("IPv4 Address found %s", ipv4_addr);

    return 0;
}

static int server_connect(void)
{
    int err;
    // Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        LOG_ERR("Failed to create socket: %d.", errno);
        return -errno;
    }

    // Connect the socket to the server
    err = connect(sock, (struct sockaddr *)&server,
                  sizeof(struct sockaddr_in));
    if (err < 0)
    {
        LOG_ERR("Connect failed : %d", errno);
        return -errno;
    }
    LOG_INF("Successfully connected to server");

    return 0;
}

int p100_socket_send(const void *buf, size_t len)
{
    return send(sock, buf, len, 0);
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    switch (has_changed)
    {
    case DK_BTN1_MSK:
        // Call send() when button 1 is pressed
        if (button_state & DK_BTN1_MSK)
        {
            int err = p100_socket_send(MESSAGE_TO_SEND, SSTRLEN(MESSAGE_TO_SEND));
            if (err < 0)
            {
                LOG_INF("Failed to send message, %d", errno);
                return;
            }
            LOG_INF("Successfully sent message: %s", MESSAGE_TO_SEND);
        }
        break;
    }
}

static void p100_socket_thread()
{
    int received;

    LOG_INF("Press button 1 send your message");

    while (1)
    {
        received = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);

        if (received < 0)
        {
            LOG_ERR("Socket error: %d, exit", errno);
            break;
        }

        if (received == 0)
        {
            LOG_ERR("Empty datagram");
            break;
        }

        recv_buf[received] = 0;
        LOG_INF("Data received from the server: (%s)", recv_buf);
    }

    (void)close(sock);
}

int p100_socket_init()
{

    if (dk_buttons_init(button_handler) != 0)
    {
        LOG_ERR("Failed to initialize the buttons library");
    }

    if (server_resolve() != 0)
    {
        LOG_INF("Failed to resolve server name");
        return -1;
    }

    if (server_connect() != 0)
    {
        LOG_INF("Failed to initialize client");
        return -1;
    }

    // Create the thread
    k_thread_create(
        &p100_socket_thread_data, p100_socket_stack_area,
        K_THREAD_STACK_SIZEOF(p100_socket_stack_area),
        p100_socket_thread,
        NULL, NULL, NULL,
        P100_SOCKET_PRIORITY, 0, K_NO_WAIT);

    return 0;
}