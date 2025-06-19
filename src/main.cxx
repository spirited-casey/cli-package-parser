// stl
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// boost
#include <boost/program_options.hpp>

namespace po = boost::program_options;

// local
#include "PackageParser/packageparser.hpp"

using namespace train_protocol;

void print_help_option(const po::options_description& desc)
{
    std::cout << "Usage: cli_package_parser [inputfile]\n\n";
    std::cout << desc << "\n";
}

int main(int argc, char** argv)
{
    po::options_description desc("Allowed options");
    fs::path in_path_opt;

    desc.add_options()
        ("help", "Produce help message.")
        ("in,I", po::value<fs::path>(&in_path_opt), "inputfile with compressed packages");

    try
    {
        po::positional_options_description p;
        po::variables_map opt;

        p.add("in", -1);
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), opt);
        po::notify(opt);

        if (opt.count("help"))
        {
            print_help_option(desc);

            return 0;
        }
    }
    catch (const po::error& e)
    {
        std::cerr << "Error: " << e.what() << "\n\n";
        print_help_option(desc);

        return -1;
    }

    if (!fs::exists(in_path_opt))
    {
        std::cerr << "Error: file doesn't exist.\npath to file: " << in_path_opt << '\n';
        return -1;
    }

    try
    {
        TrainPacketsParser parsed_packets;

        parsed_packets.Parse(in_path_opt);
        std::cout << parsed_packets;
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}