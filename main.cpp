#include "main.h"

//--------------------------------
// K2, the chess engine
// Author: Sergey Meus (serg_meus@mail.ru)
// Krasnoyarsk Krai, Russia
// 2012-2017
//--------------------------------





//--------------------------------
int main(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    k2chess *chess = new k2chess();

    delete chess;
}
