#include <iostream>
#include <vector>
#include <string>
#include <thread>

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::uint32_t room_count = 0;
std::uint32_t client_count = 0;

class Client
{
public:
    Client(tcp::socket *socket) : m_socket(socket), m_endpoint(socket->remote_endpoint()) {}

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

    bool operator==(const Client &other) const
    {
        return (m_id == other.m_id && m_endpoint == other.m_endpoint);
    }

    tcp::socket *m_socket;
    boost::asio::ip::tcp::endpoint m_endpoint;

    std::string m_name;
    std::uint32_t m_id;
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

enum REQUESTED_METHOD
{
    UNMAPED_METHOD = 0,
    LIST_OPEN_ROOMS = 1,
    FOWARD_MESSAGE = 2,
    CONNECT_TO_ROOM = 3,
    EXIT_FROM_ROOM = 4
};

class Response
{
public:
    Client *client;

    std::string message;

    REQUESTED_METHOD requested_method;
};

std::vector<Room> room_list;
std::vector<Client> client_list;

void forward_welcome_message(Client &sender_client, Room &receiver_room)
{
    boost::asio::ip::tcp::endpoint sender_endpoint = sender_client.m_endpoint;

    for (Client receiver_client : receiver_room.m_client_list)
        if (receiver_client.m_endpoint == sender_endpoint)
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='green'>SERVER</font>]: Welcome " + receiver_client.m_name + " ! You have connected to [" + receiver_room.m_name + "] !\n"));
        else
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='green'>SERVER</font>]: Client [" + sender_client.m_name + "] have connected !\n"));
}

void connection_handshake(tcp::socket *socket)
{
    Client new_client(socket);

    boost::asio::streambuf user_buffer;
    boost::asio::read_until(*socket, user_buffer, '\n');

    std::string user_name = boost::asio::buffer_cast<const char *>(user_buffer.data());

    user_name.erase(std::remove(user_name.begin(), user_name.end(), '\n'), user_name.cend());

    new_client.m_id = client_count++;
    new_client.m_name = user_name;

    client_list.push_back(new_client);
}

void remove_client(Client &client)
{
    boost::asio::ip::tcp::endpoint disconnected_endpoint = client.m_endpoint;

    bool client_found = false;
    std::vector<Client>::iterator room_client_it = room_list[client.m_room_id - 1].m_client_list.begin();

    for (Client other_client : room_list[client.m_room_id - 1].m_client_list)
    {
        if (other_client.m_id != client.m_id)
        {
            boost::asio::write(*(other_client.m_socket), boost::asio::buffer("[<font color='green'>SERVER</font>]: Client [" + client.m_name + "] have disconnected !\n"));

            if (!client_found)
            {
                room_client_it++;
            }
        }
        else
        {
            client_found = true;
        }
    }

    room_list[client.m_room_id - 1].m_client_list.erase(room_client_it);

    client.m_socket->close();

    std::vector<Client>::iterator client_it = client_list.begin();

    for (Client other_client : client_list)
    {
        if (other_client.m_id != client.m_id)
        {
            client_it++;
        }
        else
        {
            break;
        }
    }

    client_list.erase(client_it);
}

void connect_to_room(Client& sender_client, std::string response_message)
{
    uint32_t room_id = std::stoul(response_message);

    for (Room room : room_list)
    {
        if (room_id == room.m_id)
        {
            sender_client.m_room_id = room.m_id;

            std::cout << "[SERVER]: Client [" << sender_client.m_socket->remote_endpoint() << "]-[" << sender_client.m_name << "] have connected to [" << room.m_name << "]" << std::endl;

            room_list[room.m_id - 1].m_client_list.push_back(sender_client);

            forward_welcome_message(sender_client, room_list[room.m_id - 1]);
        }
    }
}

void exit_from_room(Client *sender_client)
{
}

void forward_message(Client *sender_client, std::string response_message)
{
    boost::asio::ip::tcp::endpoint sender_endpoint = sender_client->m_endpoint;

    Room receiver_room = room_list[sender_client->m_room_id - 1];

    std::cout << "[" << receiver_room.m_name << "]"
              << "-[" << sender_endpoint << "]"
              << "-[" << sender_client->m_name << "]: " << response_message << std::endl;

    for (Client receiver_client : receiver_room.m_client_list)
        if (receiver_client.m_endpoint == sender_endpoint)
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='blue'>" + receiver_client.m_name + "</font>]: " + response_message + "\n"));
        else
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='red'>" + sender_client->m_name + "</font>]: " + response_message + "\n"));
}

void list_open_rooms(tcp::socket *socket)
{
    std::string room_list_str;

    for (Room room : room_list)
        room_list_str += "ID:" + std::to_string(room.m_id) + ";Name:" + room.m_name + ";Size:" + std::to_string(room.m_size) + ";Connected:" + std::to_string(room.m_client_list.size()) + "\n";

    boost::asio::write(*socket, boost::asio::buffer(room_list_str + "\n"));
}

void response_handler(Response response)
{
    switch (response.requested_method)
    {
        case REQUESTED_METHOD::LIST_OPEN_ROOMS:
            list_open_rooms(response.client->m_socket);
            break;
        case REQUESTED_METHOD::FOWARD_MESSAGE:
            forward_message(response.client, response.message);
            break;
        case REQUESTED_METHOD::CONNECT_TO_ROOM:
            connect_to_room(*response.client, response.message);
            break;
        case REQUESTED_METHOD::EXIT_FROM_ROOM:
            exit_from_room(response.client);
            break;
    }
}

REQUESTED_METHOD map_requested_method(std::string requested_method)
{
    if(requested_method == "LIST_OPEN_ROOMS")
        return REQUESTED_METHOD::LIST_OPEN_ROOMS;
    else if(requested_method == "FOWARD_MESSAGE")
        return REQUESTED_METHOD::FOWARD_MESSAGE;
    else if(requested_method == "CONNECT_TO_ROOM")
        return REQUESTED_METHOD::CONNECT_TO_ROOM;
    else if(requested_method == "EXIT_FROM_ROOM")
        return REQUESTED_METHOD::EXIT_FROM_ROOM;
    else
        return REQUESTED_METHOD::UNMAPED_METHOD;
}

void parse_response(Response *response, std::string message)
{
    std::stringstream str_stream(message);

    std::string requested_method;
    std::string response_message;

    str_stream >> requested_method;
    str_stream >> response_message;

    response->requested_method = map_requested_method(requested_method);
    response->message = response_message;
}

Response listen_client(tcp::socket *socket)
{
    Response socket_response;

    boost::asio::streambuf response_buffer;

    boost::asio::read_until(*socket, response_buffer, "\n");

    std::string response_message = boost::asio::buffer_cast<const char *>(response_buffer.data());

    response_message.erase(std::remove(response_message.begin(), response_message.end(), '\n'), response_message.cend());

    for (Client client : client_list)
        if (client.m_socket == socket)
            socket_response.client = &client;

    parse_response(&socket_response, response_message);

    return socket_response;
}

void new_session(tcp::socket *socket)
{
    const boost::asio::ip::tcp::endpoint temp_endpoint = socket->remote_endpoint();

    try
    {
        connection_handshake(socket);

        while (true)
        {
            Response client_response = listen_client(socket);

            response_handler(client_response);
        }
    }
    catch (const boost::system::system_error &system_error)
    {
        std::cout << "[SERVER]: Connection closed with [" << temp_endpoint << "]" << std::endl;

        for (Client &client : client_list)
        {
            if (client.m_endpoint == temp_endpoint)
            {
                remove_client(client);
                break;
            }
        }

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
            tcp::socket *socket = new tcp::socket(io_service);

            acceptor.accept(*socket);

            std::cout << "[SERVER]: Connection established with [" << socket->remote_endpoint() << "]" << std::endl;

            std::thread(new_session, socket).detach();
        }
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}