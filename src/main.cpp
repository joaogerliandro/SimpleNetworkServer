#include <standard_libs.h>
#include <common.h>

using boost::asio::ip::tcp;
using namespace nlohmann;

std::uint32_t room_count = 0;
std::uint32_t client_count = 0;

boost::asio::ip::tcp::endpoint server_endpoint;

enum MESSAGE_TYPE
{
    HANDSHAKING,
    FORWARD
};

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

class Message
{
    public:
        Message(std::string message_str)
        {
            json message_json = json::parse(message_str);

            m_sender_ipaddress = message_json["SenderIpAddress"];
            m_sender_portnumber = message_json["SenderPortNumber"];
            m_content = message_json["Content"];
            m_message_type = message_json["MessageType"];
        }

        Message(std::string sender_ipaddress, std::uint32_t sender_portnumber, std::string content, MESSAGE_TYPE message_type) : 
            m_sender_ipaddress(sender_ipaddress),
            m_sender_portnumber(sender_portnumber),
            m_content(content),
            m_message_type(message_type)
        {}

        std::string to_string()
        {
            json message_json = {
                {"SenderIpAddress", m_sender_ipaddress},
                {"SenderPortNumber", m_sender_portnumber},
                {"Content", m_content},
                {"MessageType", m_message_type}
            };

            return message_json.dump();
        }

        std::string m_sender_ipaddress;
        std::uint32_t m_sender_portnumber;
        std::string m_content;
        MESSAGE_TYPE m_message_type;

        // date_time m_send_datetime
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

    for (Client receiver_client : receiver_room.m_client_list)
        if (receiver_client.m_endpoint == sender_endpoint)
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='green'>SERVER</font>]: Welcome " + receiver_client.m_name + " ! You have connected to [" + receiver_room.m_name + "] !\n"));
        else
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='green'>SERVER</font>]: Client [" + sender_client.m_name + "] have connected !\n"));
}

void connection_handshake(tcp::socket *socket, std::string handshake_content_str)
{
    Client new_client(socket);
    json handshake_content;
    
    handshake_content = json::parse(handshake_content_str);

    std::string client_username = handshake_content["Username"];

    Message open_room_list_message(server_endpoint.address().to_v4().to_string(), server_endpoint.port(), list_open_rooms(), MESSAGE_TYPE::HANDSHAKING);

    boost::asio::write(*socket, boost::asio::buffer(open_room_list_message.to_string() + "\n"));

    bool handshake_is_over = false;

    while (true)
    {
        boost::asio::streambuf handshake_buffer;

        boost::asio::read_until(*socket, handshake_buffer, "\n");

        std::string handshake_response_str = boost::asio::buffer_cast<const char *>(handshake_buffer.data());

        handshake_response_str.erase(std::remove(handshake_response_str.begin(), handshake_response_str.end(), '\n'), handshake_response_str.cend());

        Message handshake_response_message(handshake_response_str);

        json handshake_content = json::parse(handshake_response_message.m_content);

        for (Room room : room_list)
        {
            if (handshake_content["RoomId"] == room.m_id)
            {
                new_client.m_id = client_count++;
                new_client.m_name = client_username;
                new_client.m_room_id = room.m_id;

                std::cout << "[SERVER]: Client [" << socket->remote_endpoint() << "]-[" << client_username << "] have connected to [" << room.m_name << "]" << std::endl;

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

Message listen_client(tcp::socket *socket)
{
    boost::asio::streambuf response_buffer;

    boost::asio::read_until(*socket, response_buffer, "\n");

    std::string response_message = boost::asio::buffer_cast<const char *>(response_buffer.data());

    response_message.erase(std::remove(response_message.begin(), response_message.end(), '\n'), response_message.cend());

    return Message(response_message);
}

void remove_client(Client &client)
{
    boost::asio::ip::tcp::endpoint disconnected_endpoint = client.m_endpoint;

    bool client_found = false;
    std::vector<Client>::iterator room_client_it = room_list[client.m_room_id - 1].m_client_list.begin();

    for(Client other_client : room_list[client.m_room_id - 1].m_client_list)
    {
        if(other_client.m_id != client.m_id)
        {
            boost::asio::write(*(other_client.m_socket), boost::asio::buffer("[<font color='green'>SERVER</font>]: Client [" + client.m_name + "] have disconnected !\n"));
            
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

    Room receiver_room = room_list[sender_client.m_room_id - 1];

    std::cout << "[" << receiver_room.m_name << "]"
              << "-[" << sender_endpoint << "]"
              << "-[" << sender_client.m_name << "]: " << response_message << std::endl;

    for (Client receiver_client : receiver_room.m_client_list)
        if (receiver_client.m_endpoint == sender_endpoint)
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='blue'>" + receiver_client.m_name + "</font>]: " + response_message + "\n"));
        else
            boost::asio::write(*receiver_client.m_socket, boost::asio::buffer("[<font color='red'>" + sender_client.m_name + "</font>]: " + response_message + "\n"));
}

void new_session(tcp::socket *socket)
{
    const boost::asio::ip::tcp::endpoint temp_endpoint = socket->remote_endpoint();

    try
    {        
        while (true)
        {
            Message response_message = listen_client(socket);

            switch (response_message.m_message_type)
            {
                case HANDSHAKING:
                    connection_handshake(socket, response_message.m_content);
                    break;
                case FORWARD:
                    for (Client client : client_list)
                        if (client.m_socket == socket)
                            forward_message(client, response_message.m_content); 
                    break;

                default:
                    break;
            }
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
        server_endpoint = acceptor.local_endpoint();

        std::cout << "[SERVER]: This server is running in this endpoint: [" << server_endpoint << "]" << std::endl;
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