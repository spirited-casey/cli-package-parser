// stl
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// boost
#include <boost/program_options.hpp>

namespace po = boost::program_options;

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

    std::fstream in(in_path_opt, std::ios::in);

    if (!in.is_open())
    {
        std::cerr << "Couldn't open file: " << in_path_opt << '\n';
        return -1;
    }

    return 0;
}