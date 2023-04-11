#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

int main(int argc, char *argv[])
{
    try
    {
        boost::asio::io_service io_service;

        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 4444));
        std::cout << "[SERVER]: Awaiting connections ..." << std::endl;

        for(;;)
        {
            tcp::socket socket(io_service);

            acceptor.accept(socket);
            std::cout << "[SERVER]: New connection established" << std::endl;

            std::string message = "Connected successfully !\n";

            boost::system::error_code ignored_error;

            boost::asio::write(socket, boost::asio::buffer(message), ignored_error);
        }
    }
    catch(std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}