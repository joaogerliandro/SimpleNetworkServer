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

    bool operator==(const Client& other) const
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

std::vector<Room> room_list;
std::vector<Client> client_list;

std::string list_open_rooms()
{
    std::string room_list_str;

    for (Room room : room_list)
        room_list_str += "ID:" + std::to_string(room.m_id) + ";Name:" + room.m_name + ";Size:" + std::to_string(room.m_size) + ";Connected:" + std::to_string(room.m_client_list.size()) + "\n";

    return room_list_str;
}

void forward_welcome_message(Client &sender_client, Room &receiver_room)
{
    boost::asio::ip::tcp::endpoint sender_endpoint = sender_client.m_endpoint;

    std::string sender_ip = sender_endpoint.address().to_string() + ":" + std::to_string(sender_endpoint.port());

    for (Client receiver_client : receiver_room.m_client_list)
        if (receiver_client.m_endpoint == sender_endpoint)
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='green'>SERVER</font>]: Welcome ! You have connected to [" + receiver_room.m_name + "] !\n"));
        else
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='green'>SERVER</font>]: Client [" + sender_ip + "] have connected !\n"));
}

void connection_handshake(tcp::socket *socket)
{
    Client new_client(socket);

    boost::asio::write(*socket, boost::asio::buffer(list_open_rooms() + "\n"));

    bool handshake_is_over = false;

    while (true)
    {
        boost::asio::streambuf handshake_buffer;

        boost::asio::read_until(*socket, handshake_buffer, "\n");

        std::string handshake_message = boost::asio::buffer_cast<const char *>(handshake_buffer.data());

        handshake_message.erase(std::remove(handshake_message.begin(), handshake_message.end(), '\n'), handshake_message.cend());

        for (Room room : room_list)
        {
            if (std::stoul(handshake_message) == room.m_id)
            {
                new_client.m_id = client_count++;
                new_client.m_room_id = room.m_id;

                std::cout << "[SERVER]: Client [" << socket->remote_endpoint() << "] have connected to [" << room.m_name << "]" << std::endl;

                client_list.push_back(new_client);
                room_list[room.m_id - 1].m_client_list.push_back(new_client);

                forward_welcome_message(new_client, room_list[room.m_id - 1]);

                handshake_is_over = true;
                break;
            }
        }

        if (handshake_is_over)
            break;
    }
}

std::string listen_client(tcp::socket *socket)
{
    boost::asio::streambuf response_buffer;

    boost::asio::read_until(*socket, response_buffer, "\n");

    std::string response_message = boost::asio::buffer_cast<const char *>(response_buffer.data());

    response_message.erase(std::remove(response_message.begin(), response_message.end(), '\n'), response_message.cend());

    return response_message;
}

void remove_client(Client &client)
{
    boost::asio::ip::tcp::endpoint disconnected_endpoint = client.m_endpoint;
    std::string disconnected_ip = disconnected_endpoint.address().to_string() + ":" + std::to_string(disconnected_endpoint.port());

    bool client_found = false;
    std::vector<Client>::iterator room_client_it = room_list[client.m_room_id - 1].m_client_list.begin();

    for(Client other_client : room_list[client.m_room_id - 1].m_client_list)
    {
        if(other_client.m_id != client.m_id)
        {
            boost::asio::write(*(other_client.m_socket), boost::asio::buffer("[<font color='green'>SERVER</font>]: Client [" + disconnected_ip + "] have disconnected !\n"));
            
            if(!client_found)
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

    for(Client other_client : client_list)
    {
        if(other_client.m_id != client.m_id)
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

void forward_message(Client &sender_client, std::string response_message)
{
    boost::asio::ip::tcp::endpoint sender_endpoint = sender_client.m_endpoint;

    std::string sender_ip = sender_endpoint.address().to_string() + ":" + std::to_string(sender_endpoint.port());

    Room receiver_room = room_list[sender_client.m_room_id - 1];

    std::cout << "[" << receiver_room.m_name << "]"
              << "-[" << sender_endpoint << "]: " << response_message << std::endl;

    for (Client receiver_client : receiver_room.m_client_list)
        if (receiver_client.m_endpoint == sender_endpoint)
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='blue'>LOCALHOST</font>]: " + response_message + "\n"));
        else
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='red'>" + sender_ip + "</font>]: " + response_message + "\n"));
}

void new_session(tcp::socket *socket)
{
    const boost::asio::ip::tcp::endpoint temp_endpoint = socket->remote_endpoint();

    try
    {
        connection_handshake(socket);

        while (true)
        {
            std::string response_message = listen_client(socket);

            for (Client client : client_list)
                if (client.m_socket == socket)
                    forward_message(client, response_message);
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