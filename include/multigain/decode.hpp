#ifndef MULTIGAIN_DECODE_HPP
#define MULTIGAIN_DECODE_HPP

#include <fstream>

namespace multigain {

void	decode_mp3_frame(std::ifstream &, double *left, double *right);

}

#endif
