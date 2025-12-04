// Copyright (C) 2023 Advanced Micro Devices, Inc
//
// SPDX-License-Identifier: MIT

#include <iostream>
#include <fstream>
#include "graph.hpp"

simpleGraph vadd_graph;

#if defined(__AIESIM__) || defined(__X86SIM__)
int main(int argc, char** argv) {
    vadd_graph.init();
#ifdef STREAM
    vadd_graph.run(512);
#else
   int total_vectors = 256; // for example
    int block_size = 128;
    int num_blocks = total_vectors / block_size;

    for (int i = 0; i < num_blocks; ++i)
    {
        // update input file pointers to load next 128 vectors
        std::string in1_filename = "../data/input1_block" + std::to_string(i) + ".txt";
        std::ofstream in1_block(in1_filename);
        // write 128 vectors from global memory or file

        // update PLIO source dynamically if needed
        vadd_graph.p_s1 = input_plio::create("StreamIn1", plio_32_bits, in1_filename.c_str());

        vadd_graph.run(1);
        vadd_graph.wait();
    }
    vadd_graph.end();
#endif
    vadd_graph.end();
    std::ifstream golden_file, aie_file;
    golden_file.open("../data/golden.txt");
    if(golden_file.fail()){
      std::cerr << "Error opening golden file." << std::endl;
      golden_file.close();
      return -1;
    }

#if defined(__X86SIM__)
    aie_file.open("x86simulator_output/output.txt");
#else
    aie_file.open("aiesimulator_output/output.txt");
#endif
    if(aie_file.fail()){
      std::cerr<<"Error opening produced file."<< std::endl;
      return -1;
    }

    std::string line_golden, line_aie;
    bool match = true;
    while (getline(golden_file, line_golden)){
        getline(aie_file, line_aie);
        if (aie_file.eof()){
            std::cerr << "AI Engine results are too short to match golden result" << std::endl;
            match = false;
            break;
        }
        while (line_aie[0]=='T')
            getline(aie_file, line_aie);
        if (std::stoi(line_golden) != std::stoi(line_aie)){
            match = false;
            break;
        }
    }
    if (!aie_file.eof()){
        getline(aie_file, line_aie);
        if (!aie_file.eof())
            std::cerr << "AI Engine results are too long to match golden result" << std::endl;
    }
    if (match)
        std::cout << "AI Engine results match golden result" << std::endl;
    else
        std::cout << "AI Engine results DO NOT match golden result" << std::endl;

    golden_file.close();
    aie_file.close();
    return 0;
}
#endif