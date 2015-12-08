#pragma once

// Defines some macros to help debugging

#ifdef WANTS_POPART_LOGGING
#define POPART_COUT(x) std::cout << x << std::endl
#define POPART_CERR(x) std::cerr << x << std::endl
#define POPART_COUT_DEBUG(x) std::cout << x << std::endl
#else
#define POPART_COUT(x) 
#define POPART_CERR(x) 
#define POPART_COUT_DEBUG(x)
#endif
