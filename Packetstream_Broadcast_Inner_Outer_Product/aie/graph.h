#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <adf.h>
#include "kernels.h"
#include <string.h>

#define N 6
#define MAXROW 8

using namespace adf;

class simpleGraph : public graph {
private:
    kernel core[N];
    pktsplit<N> sp;         

public:

    input_plio p_s0;                     
    input_plio p_s1;                       
    output_plio p_s2_0, p_s2_1, p_s2_2, p_s2_3, p_s2_4, p_s2_5;

    simpleGraph() {
        int col = 1;
        output_plio* p_s2_list[N] = {&p_s2_0, &p_s2_1, &p_s2_2, &p_s2_3, &p_s2_4, &p_s2_5};

        
        p_s0 = input_plio::create("StreamIn0", plio_32_bits, "data/input0.seq");
        p_s1 = input_plio::create("StreamIn1_broadcast", plio_32_bits, "data/input1.txt");

        for (int i = 0; i < N; i++) {
            char out_file_str[30];
            sprintf(out_file_str, "output%d", i);
            char p2_name[30];
            sprintf(p2_name, "StreamOut0_%d", i);

            *p_s2_list[i] = output_plio::create(p2_name, plio_32_bits, out_file_str);

            core[i] = kernel::create(aie_core1);
            source(core[i]) = "aie_core1.cpp";
            runtime<ratio>(core[i]) = 1;
            col = (i / MAXROW) * 10;
            location<kernel>(core[i]) = tile(col, i % MAXROW);


            connect<pktstream>(core[i].out[0], (*p_s2_list[i]).in[0]);
        }

        sp = pktsplit<N>::create();

        connect<pktstream>(p_s0.out[0], sp.in[0]);
        for (int i = 0; i < N; i++) {
            connect<pktstream>(sp.out[i], core[i].in[0]);
        }

        for (int i = 0; i < N; ++i) {
            connect<stream>(p_s1.out[0], core[i].in[1]);
        }
    }
};

#endif  
