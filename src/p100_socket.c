
#include <zephyr/kernel.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/modem/hl7800.h>

#include "p100_socket.h"

LOG_MODULE_REGISTER(p100_socket_log, LOG_LEVEL_INF);

// Hostname and port for the echo server
#define SERVER_HOSTNAME "nordicecho.westeurope.cloudapp.azure.com"
#define SERVER_IP "20.56.165.163"
// #define SERVER_IP "45.79.112.203"
#define SERVER_PORT_INT 2444
// #define SERVER_IP "0.0.0.0"
#define SERVER_PORT "2444"

#define MESSAGE_SIZE 256
#define MESSAGE_TO_SEND "Hello from Pinnacle 100"
#define SSTRLEN(s) (sizeof(s) - 1)

// Structure for the socket and server address
static int sock;
static struct sockaddr_storage server;
static struct dns_resolve_context *dns;

// Buffer for receiving from server
static uint8_t recv_buf[MESSAGE_SIZE];

static int server_resolve(void)
{
    int ret = 0;

    // Retrieve the relevant information from the result structure
    struct sockaddr_in *server4 = ((struct sockaddr_in *)&server);

    inet_pton(AF_INET, SERVER_IP, &server4->sin_addr.s_addr);
    server4->sin_family = AF_INET;

    // Flip the port because of how it is being cast
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

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    switch (has_changed)
    {
    case DK_BTN1_MSK:
        // Call send() when button 1 is pressed
        if (button_state & DK_BTN1_MSK)
        {
            int err = send(sock, MESSAGE_TO_SEND, SSTRLEN(MESSAGE_TO_SEND), 0);
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

int p100_socket_init()
{
    int received;

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

    return -1;
}