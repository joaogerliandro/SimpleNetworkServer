#include <iostream>
#include <vector>
#include <string>
#include <thread>

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Client
{
    public:
        Client(tcp::socket& socket) : m_socket(socket) {}

        tcp::socket& m_socket;
};

class Room
{
    public:
        Room(std::string room_name, std::uint8_t room_size) : m_name(room_name), m_size(room_size) {}
    
        std::uint8_t m_size;
        std::string m_name;

    private:
        std::vector<Client> m_client_list;
};

std::vector<Room> room_list;
std::vector<Client> client_list;

std::string list_open_rooms()
{
    std::string room_list_str;

    for(Room room : room_list)
        room_list_str += "\n- [" + room.m_name + "] is open with " + std::to_string(room.m_size) + " spaces !";

    return room_list_str;
}

void connection_handshake(tcp::socket& socket)
{
    client_list.push_back(Client(socket));
    
    std::string handshake_message = "[SERVER]: Welcome " + socket.remote_endpoint().address().to_string() + ":" + std::to_string(socket.remote_endpoint().port()) + " !";

    handshake_message += "\n[SERVER]: Open rooms in the Server: " + list_open_rooms();

    boost::asio::write(socket, boost::asio::buffer(handshake_message + "\n"));
}

void new_session(tcp::socket socket)
{
    connection_handshake(socket);

    for (;;)
    {
        boost::asio::streambuf response_buffer;

        boost::asio::read_until(socket, response_buffer, "\n");

        std::string response_message = boost::asio::buffer_cast<const char *>(response_buffer.data());

        response_message.erase(std::remove(response_message.begin(), response_message.end(), '\n'), response_message.cend());

        std::cout << "[" << socket.remote_endpoint() << "]: " << response_message << std::endl;

        boost::asio::ip::tcp::endpoint sender_endpoint = socket.remote_endpoint();

        std::string sender_ip = sender_endpoint.address().to_string() + ":" + std::to_string(sender_endpoint.port());

        for(Client client : client_list)
        {
            std::string global_mensage;

            if(client.m_socket.remote_endpoint().address() == sender_endpoint.address() &&
               client.m_socket.remote_endpoint().port() == sender_endpoint.port())
                global_mensage = "[LOCALHOST]: " + response_message;
            else
                global_mensage = "[" + sender_ip + "]: " + response_message;
            
            boost::asio::write(client.m_socket, boost::asio::buffer(global_mensage + "\n"));
        }
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

        for (;;)
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