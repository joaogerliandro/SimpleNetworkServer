#include <iostream>
#include <vector>
#include <string>
#include <thread>

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::uint32_t room_count = 0;

class Client
{
public:
    Client(tcp::socket &socket) : m_socket(socket), m_endpoint(socket.remote_endpoint()) {}

    Client(const Client &) = default;

    Client &operator=(Client &&other)
    {
        if (this != &other)
        {
            m_socket = std::move(other.m_socket);
            m_room_id = std::move(other.m_room_id);
        }
        return *this;
    }

    tcp::socket &m_socket;
    boost::asio::ip::tcp::endpoint m_endpoint;

    std::string m_name;
    std::uint32_t m_room_id;
};

class Room
{
public:
    Room(std::string room_name, std::uint8_t room_size) : m_name(room_name), m_size(room_size), m_id(++room_count) {}

    std::uint32_t m_id;
    std::uint8_t m_size;
    std::string m_name;
    std::vector<Client> m_client_list;
};

std::vector<Room> room_list;
std::vector<Client> client_list;

std::string list_open_rooms()
{
    std::string room_list_str;

    for (Room room : room_list)
        room_list_str += "\n- [" + room.m_name + "] ID:" + std::to_string(room.m_id) + " is open with " + std::to_string(room.m_size) + " spaces !";

    return room_list_str;
}

void connection_handshake(tcp::socket &socket)
{
    Client new_client(socket);

    std::string handshake_message = "[SERVER]: Welcome " + socket.remote_endpoint().address().to_string() + ":" + std::to_string(socket.remote_endpoint().port()) + " !";

    handshake_message += "\n[SERVER]: Open rooms in the Server: " + list_open_rooms();

    boost::asio::write(socket, boost::asio::buffer(handshake_message + "\n"));

    bool handshake_is_over = false;

    while (true)
    {
        boost::asio::streambuf handshake_buffer;

        boost::asio::read_until(socket, handshake_buffer, "\n");

        handshake_message = boost::asio::buffer_cast<const char *>(handshake_buffer.data());

        handshake_message.erase(std::remove(handshake_message.begin(), handshake_message.end(), '\n'), handshake_message.cend());

        for (Room room : room_list)
        {
            if (std::stoul(handshake_message) == room.m_id)
            {
                new_client.m_room_id = room.m_id;

                boost::asio::write(socket, boost::asio::buffer("[SERVER]: You have connected to " + room.m_name + "!\n"));

                client_list.push_back(new_client);
                room_list[room.m_id - 1].m_client_list.push_back(new_client);

                handshake_is_over = true;
                break;
            }
        }

        if (handshake_is_over)
            break;

        boost::asio::write(socket, boost::asio::buffer("NOK\n"));
    }
}

std::string listen_client(tcp::socket &socket)
{
    boost::asio::streambuf response_buffer;

    boost::asio::read_until(socket, response_buffer, "\n");

    std::string response_message = boost::asio::buffer_cast<const char *>(response_buffer.data());

    response_message.erase(std::remove(response_message.begin(), response_message.end(), '\n'), response_message.cend());

    return response_message;
}

void forward_message(Client &sender_client, std::string response_message)
{
    boost::asio::ip::tcp::endpoint sender_endpoint = sender_client.m_endpoint;

    std::string sender_ip = sender_endpoint.address().to_string() + ":" + std::to_string(sender_endpoint.port());

    Room receiver_room = room_list[sender_client.m_room_id - 1];

    std::cout << "[" << receiver_room.m_name << "]"
              << "-[" << sender_endpoint << "]: " << response_message << std::endl;

    for (Client receiver_client : receiver_room.m_client_list)
        if (receiver_client.m_endpoint == sender_endpoint)
            boost::asio::write(receiver_client.m_socket, boost::asio::buffer("[LOCALHOST]: " + response_message + "\n"));
        else
            boost::asio::write(receiver_client.m_socket, boost::asio::buffer("[" + sender_ip + "]: " + response_message + "\n"));
}

void new_session(tcp::socket socket)
{
    boost::asio::ip::tcp::endpoint temp_endpoint = socket.remote_endpoint();

    try
    {
        connection_handshake(socket);

        while (true)
        {
            std::string response_message = listen_client(socket);

            for (Client client : client_list)
                if (client.m_endpoint == socket.remote_endpoint())
                    forward_message(client, response_message);
        }
    }
    catch (const boost::system::system_error &system_error)
    {
        for (Client client : client_list)
        {
            if (client.m_endpoint == temp_endpoint)
            {
                room_list[client.m_room_id - 1].m_client_list.erase(std::remove_if(client_list.begin(), client_list.end(),
                                                                                   [&](const Client &client)
                                                                                   {
                                                                                       return client.m_endpoint == temp_endpoint;
                                                                                   }),
                                                                    client_list.end());
            }
        }

        client_list.erase(std::remove_if(client_list.begin(), client_list.end(),
                                         [&](const Client &client)
                                         {
                                             return client.m_endpoint == temp_endpoint;
                                         }),
                          client_list.end());

        std::cout << "[SERVER]: Connection closed with [" << temp_endpoint << "]" << std::endl;

        std::cout << "[SERVER]: Connected clients: " << client_list.size() << std::endl;
    }
}

int main(int argc, char *argv[])
{
    try
    {
        room_list.push_back(Room(std::string("DEFAULT-ROOM"), 8));
        room_list.push_back(Room(std::string("PRIVATE-ROOM"), 2));

        boost::asio::io_service io_service;

        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 4444));
        std::cout << "[SERVER]: Awaiting connections ..." << std::endl;

        while (true)
        {
            tcp::socket socket(io_service);

            acceptor.accept(socket);

            std::cout << "[SERVER]: Connection established with [" << socket.remote_endpoint() << "]" << std::endl;

            std::thread(new_session, std::move(socket)).detach();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}