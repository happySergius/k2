#include "main.h"

//--------------------------------
// K2, the chess engine
// Author: Sergey Meus (serg_meus@mail.ru)
// Krasnoyarsk Kray, Russia
// Copyright 2012-2016
//--------------------------------





cmdStruct commands[]
{
//   Command        Function
    {"new",         NewCommand},
    {"setboard",    SetboardCommand},
    {"set",         SetboardCommand},
    {"quit",        QuitCommand},
    {"q",           QuitCommand},
    {"perft",       PerftCommand},
    {"go",          GoCommand},
    {"level",       LevelCommand},
    {"force",       ForceCommand},
    {"sn",          SetNodesCommand},
    {"st",          SetTimeCommand},
    {"sd",          SetDepthCommand},
    {"protover",    ProtoverCommand},
    {"?",           StopCommand},
    {"result",      ResultCommand},
    {"time",        TimeCommand},
    {"eval",        EvalCommand},
    {"test",        TestCommand},
    {"fen",         FenCommand},
    {"xboard",      XboardCommand},
    {"easy",        EasyCommand},
    {"hard",        HardCommand},
    {"memory",      MemoryCommand},
    {"analyze",     AnalyzeCommand},
    {"exit",        ExitCommand},

    {"undo",        Unsupported},
    {"remove",      Unsupported},
    {"computer",    Unsupported},
    {"random",      Unsupported},
    {"post",        Unsupported},
    {"nopost",      Unsupported},
    {"random",      Unsupported},

    {"otim",        Unsupported},
    {"accepted",    Unsupported},
    {".",           Unsupported},
    {"",            Unsupported},

    {"uci",         UciCommand},
    {"setoption",   SetOptionCommand},
    {"isready",     IsReadyCommand},
    {"position",    PositionCommand},
    {"ucinewgame",  NewCommand},
    {"stop",        StopCommand},
    {"ponderhit",   PonderhitCommand},
    {"setvalue",    SetvalueCommand},
};






bool force = false;
bool quit = false;
bool pondering_enabled = false;

#ifndef DONT_USE_THREAD_FOR_INPUT
std::thread t;
// for compilers with C++11 support under Linux
// -pthread option must be used for gcc linker
#endif // USE_THREAD_FOR_INPUT





k2engine eng;


//--------------------------------
int main(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);

//    for(auto i = 0; i < NPARAMS; ++i)
//        param.push_back(1);

    eng.ClearHash();

    eng.max_search_depth = k2chess::max_ply;
    eng.time_remains = 300000000;
    eng.time_base = 300000000;
    eng.time_inc = 0;
    eng.moves_per_session = 0;
    eng.max_nodes_to_search = 0;
    eng.time_command_sent = false;

    char in[0x4000];
    while(!quit)
    {
        if(!std::cin.getline(in, sizeof(in), '\n'))
        {
            if (std::cin.eof())
                quit = true;
            else
                std::cin.clear();
        }

        if(CmdProcess((std::string)in))
        {
            // NiCheGoNeDeLaYem!
        }
        else if(!eng.busy && LooksLikeMove((std::string)in))
        {
            if(!eng.MakeMoveFinaly(in))
                std::cout << "Illegal move" << std::endl;
            else if(!force)
            {
#ifndef DONT_USE_THREAD_FOR_INPUT
                if(t.joinable())
                    t.join();
                t = std::thread(&k2engine::MainSearch, &eng);
#else
                eng.MainSearch();
#endif // USE_THREAD_FOR_INPUT
            }
        }
        else
            std::cout << "Unknown command: " << in << std::endl;
    }
    if(eng.busy)
        StopEngine();
}





//--------------------------------
bool CmdProcess(std::string in)
{
    std::string firstWord, remains;

    GetFirstArg(in, &firstWord, &remains);

    for(size_t i = 0; i < sizeof(commands) / sizeof(cmdStruct); ++i)
    {
        if(firstWord == commands[i].command)
        {
            commands[i].foo(remains);
            return true;
        }
    }
    return false;
}





//--------------------------------
void GetFirstArg(std::string in, std::string (*firstWord),
                 std::string (*remainingWords))
{
    if(in.empty())
        return;
    std::string delimiters = " ;,\t\r\n";
    *firstWord = in;
    int firstSymbol = firstWord->find_first_not_of(delimiters);
    if(firstSymbol == -1)
        firstSymbol = 0;
    *firstWord = firstWord->substr(firstSymbol, (int)firstWord->size());
    int secondSymbol = firstWord->find_first_of(delimiters);
    if(secondSymbol == -1)
        secondSymbol = (int)firstWord->size();

    *remainingWords = firstWord->substr(secondSymbol, (int)firstWord->size());
    *firstWord = firstWord->substr(0, secondSymbol);
}





//--------------------------------
bool LooksLikeMove(std::string in)
{
    if(in.size() < 4 || in.size() > 5)
        return false;
    if(in.at(0) > 'h' || in.at(0) < 'a')
        return false;
    if(in.at(1) > '8' || in.at(1) < '0')
        return false;
    if(in.at(2) > 'h' || in.at(2) < 'a')
        return false;
    if(in.at(3) > '8' || in.at(3) < '0')
        return false;

    return true;

}





//--------------------------------
void StopEngine()
{
#ifndef DONT_USE_THREAD_FOR_INPUT
    eng.stop = true;
    t.join();
#endif // USE_THREAD_FOR_INPUT
}





//--------------------------------
void NewCommand(std::string in)
{
    UNUSED(in);
    if(eng.busy)
        StopEngine();
    force = false;
    eng.pondering_in_process = false;
    if(!eng.xboard && !eng.uci)
    {
        if(eng.total_time_spent == 0)
            eng.total_time_spent = 1e-5;
        std::cout
                << "( Total node count: " << eng.total_nodes
                << ", total time spent: " << eng.total_time_spent / 1000000.0
                << " )" << std::endl
                << std::setprecision(4) << std::fixed
                << "( MNPS = " << eng.total_nodes / eng.total_time_spent
                << " )" << std::endl;
    }
    eng.InitEngine();
    eng.ClearHash();
}





//-------------------------------
void SetboardCommand(std::string in)
{
    if(eng.busy)
        return;

    auto firstSymbol = in.find_first_not_of(" \t");
    in.erase(0, firstSymbol);

    if(!eng.FenStringToEngine((char *)in.c_str()))
        std::cout << "Illegal position" << std::endl;
    else if(eng.infinite_analyze && eng.xboard)
        AnalyzeCommand(in);
}





//--------------------------------
void QuitCommand(std::string in)
{
    UNUSED(in);
#ifndef DONT_USE_THREAD_FOR_INPUT
    if(eng.busy || t.joinable())
        StopEngine();
#endif // USE_THREAD_FOR_INPUT

    quit = true;
}





//--------------------------------
void PerftCommand(std::string in)
{
    if(eng.busy)
        return;
    Timer timer;
    double tick1, tick2, deltaTick;
    timer.start();
    tick1 = timer.getElapsedTimeInMicroSec();

    eng.nodes = 0;
    eng.max_search_depth = atoi(in.c_str());
    eng.Perft(eng.max_search_depth);
    eng.max_search_depth = k2chess::max_ply;
    tick2 = timer.getElapsedTimeInMicroSec();
    deltaTick = tick2 - tick1;

    std::cout << std::endl << "nodes = " << eng.nodes << std::endl
              << "dt = " << deltaTick / 1000000. << std::endl
              << "Mnps = " << eng.nodes / (deltaTick + 1)
              << std::endl << std::endl;
}





//--------------------------------
void GoCommand(std::string in)
{
    UNUSED(in);
    if(eng.busy)
        return;

    if(eng.uci)
        UciGoCommand(in);
    else
        force = false;
#ifndef DONT_USE_THREAD_FOR_INPUT
    if(t.joinable())
        t.join();
    t = std::thread(&k2engine::MainSearch, &eng);
#else
    eng.MainSearch();
#endif // USE_THREAD_FOR_INPUT

}





//--------------------------------
void LevelCommand(std::string in)
{
    if(eng.busy)
        return;
    std::string arg1, arg2, arg3;
    GetFirstArg(in, &arg1, &arg2);
    GetFirstArg(arg2, &arg2, &arg3);
    double base, mps, inc;
    mps = atoi(arg1.c_str());

    int colon = arg2.find(':');
    if(colon != -1)
    {
        arg2.at(colon) = '.';
        int size_of_seconds = arg2.size() - colon - 1;
        base = atof(arg2.c_str());
        if(base < 0)
            base = -base;
        if(size_of_seconds == 1)
            base = 0.1*(base - (int)base) + (int)base;
        int floorBase = (int)base;
        base = (base - floorBase)*100/60 + floorBase;

    }
    else
        base = atof(arg2.c_str());

    inc = atof(arg3.c_str());

    eng.time_base = 60*1000000.*base;
    eng.time_inc = 1000000*inc;
    eng.moves_per_session = mps;
    eng.time_remains = eng.time_base;
    eng.max_nodes_to_search = 0;
    eng.max_search_depth = k2chess::max_ply;
}





//--------------------------------
void ForceCommand(std::string in)
{
    UNUSED(in);
    if(eng.busy)
        StopEngine();
    force = true;
}





//--------------------------------
void SetNodesCommand(std::string in)
{
    if(eng.busy)
        return;
    eng.time_base = 0;
    eng.moves_per_session = 0;
    eng.time_inc = 0;
    eng.max_nodes_to_search = atoi(in.c_str());
    eng.max_search_depth = k2chess::max_ply;
}





//--------------------------------
void SetTimeCommand(std::string in)
{
    if(eng.busy)
        return;
    eng.time_base = 0;
    eng.moves_per_session = 1;
    eng.time_inc = atof(in.c_str())*1000000.;
    eng.max_nodes_to_search = 0;
    eng.max_search_depth = k2chess::max_ply;
    eng.time_remains = 0;
}





//--------------------------------
void SetDepthCommand(std::string in)
{
    if(eng.busy)
        return;
    eng.max_search_depth = atoi(in.c_str());
}





//--------------------------------
void ProtoverCommand(std::string in)
{
    UNUSED(in);
    if(eng.busy)
        return;
    eng.xboard = true;
    eng.uci = false;

    std::cout << "feature "
              "myname=\"K2 v." ENGINE_VERSION "\" "
              "setboard=1 "
              "analyze=1 "
              "san=0 "
              "colors=0 "
              "pause=0 "
              "usermove=0 "
              "time=1 "
              "draw=0 "
              "sigterm=0 "
              "sigint=0 "
              "memory=1 "
              "done=1 " << std::endl;
}





//--------------------------------
void StopCommand(std::string in)
{
    UNUSED(in);
#ifndef DONT_USE_THREAD_FOR_INPUT
    if(eng.busy)
    {
        eng.stop = true;
        t.join();
    }
#endif // USE_THREAD_FOR_INPUT
}





//--------------------------------
void ResultCommand(std::string in)
{
    UNUSED(in);
    if(eng.busy)
        StopEngine();
}





//--------------------------------
void TimeCommand(std::string in)
{
    if(eng.busy)
    {
        std::cout << "telluser time command recieved while engine is busy"
                  << std::endl;
        return;
    }
    double tb = atof(in.c_str()) * 10000;
    eng.time_remains = tb;
    eng.time_command_sent = true;
}





//--------------------------------
void EvalCommand(std::string in)
{
    UNUSED(in);
    if(eng.busy)
        return;

    eng.EvalDebug();
}





//--------------------------------
void TestCommand(std::string in)
{
    UNUSED(in);
}





//--------------------------------
void FenCommand(std::string in)
{
    UNUSED(in);
    eng.ShowFen();
}





//--------------------------------
void XboardCommand(std::string in)
{
    UNUSED(in);
    eng.xboard = true;
    eng.uci = false;
    std::cout << "( build time: "
              << __DATE__ << " " << __TIME__
              << " )" << std::endl;
}





//--------------------------------
void Unsupported(std::string in)
{
    UNUSED(in);
}





//--------------------------------
void UciCommand(std::string in)
{
    UNUSED(in);
    eng.uci = true;
    std::cout << "id name K2 v." ENGINE_VERSION << std::endl;
    std::cout << "id author Sergey Meus" << std::endl;
    std::cout << "option name Hash type spin default 64 min 0 max 2048"
              << std::endl;
    std::cout << "uciok" << std::endl;
}





//--------------------------------
void SetOptionCommand(std::string in)
{
    std::string arg1, arg2;
    GetFirstArg(in, &arg1, &arg2);
    if(arg1 != "name")
        return;
    GetFirstArg(arg2, &arg1, &arg2);

    if(arg1 == "Hash" || arg1 == "hash")
    {
        GetFirstArg(arg2, &arg1, &arg2);
        if(arg1 != "value")
            return;
        GetFirstArg(arg2, &arg1, &arg2);
        auto size_MB = atoi(arg1.c_str());
        eng.ReHash(size_MB);
    }
}





//--------------------------------
void IsReadyCommand(std::string in)
{
    UNUSED(in);
    std::cout << "\nreadyok\n";  // '\n' to avoid multithreading problems
}





//--------------------------------
void PositionCommand(std::string in)
{

    std::string arg1, arg2;
    GetFirstArg(in, &arg1, &arg2);

    if(arg1 == "fen")
    {
        std::string fenstring;
        auto beg = arg2.find_first_not_of(" \t");
        fenstring = arg2.substr(beg, arg2.size());
        if(!eng.FenStringToEngine((char *)fenstring.c_str()))
        {
            std::cout << "Illegal position" << std::endl;
            return;
        }
    }
    else
        eng.InitEngine();

    int moves = arg2.find("moves");
    if(moves == -1)
        return;

    auto mov_seq = arg2.substr(moves + 5, in.size());
    auto beg = mov_seq.find_first_not_of(" \t");
    mov_seq = mov_seq.substr(beg, mov_seq.size());

    ProcessMoveSequence(mov_seq);
}





//--------------------------------
void ProcessMoveSequence(std::string in)
{
    std::string arg1, arg2;
    arg1 = in;
    while(true)
    {
        GetFirstArg(arg1, &arg1, &arg2);
        if(arg1.empty())
            break;
        if(!eng.MakeMoveFinaly((char *)arg1.c_str()))
            break;
        arg1 = arg2;
    }
}





//--------------------------------
void UciGoCommand(std::string in)
{
    eng.pondering_in_process = false;
    bool no_movestogo_arg = true;
    std::string arg1, arg2;
    arg1 = in;
    while(true)
    {
        GetFirstArg(arg1, &arg1, &arg2);
        if(arg1.empty())
            break;
        if(arg1 == "infinite")
        {
            eng.infinite_analyze = true;
            break;
        }

        else if(arg1 == "wtime" || arg1 == "btime")
        {
            char clr = arg1[0];
            GetFirstArg(arg2, &arg1, &arg2);
            if((clr == 'w' && eng.WhiteIsOnMove())
                    || (clr == 'b' && !eng.WhiteIsOnMove()))
            {
                eng.time_base = 1000.*atof(arg1.c_str());
                eng.time_remains = eng.time_base;
                eng.max_nodes_to_search = 0;
                eng.max_search_depth = k2chess::max_ply;

                // crutch: engine must know that time changed by GUI
                eng.time_command_sent = true;
            }
            arg1 = arg2;
        }
        else if(arg1 == "winc" || arg1 == "binc")
        {
            char clr = arg1[0];
            GetFirstArg(arg2, &arg1, &arg2);
            if((clr == 'w' && eng.WhiteIsOnMove())
                    || (clr == 'b' && !eng.WhiteIsOnMove()))
                eng.time_inc         = 1000.*atof(arg1.c_str());
            arg1 = arg2;
        }
        else if(arg1 == "movestogo")
        {
            GetFirstArg(arg2, &arg1, &arg2);
            eng.moves_per_session = atoi(arg1.c_str());
            arg1 = arg2;
            no_movestogo_arg = false;
        }
        else if(arg1 == "movetime")
        {
            GetFirstArg(arg2, &arg1, &arg2);
            eng.time_base = 0;
            eng.moves_per_session = 1;
            eng.time_inc = 1000.*atof(arg1.c_str());
            eng.max_nodes_to_search = 0;
            eng.max_search_depth = k2chess::max_ply;
            eng.time_remains = 0;

            arg1 = arg2;
        }
        else if(arg1 == "depth")
        {
            GetFirstArg(arg2, &arg1, &arg2);
            eng.max_search_depth = atoi(arg1.c_str());
            eng.time_base = INFINITY;
            eng.time_remains = eng.time_base;
            eng.max_nodes_to_search = 0;

            arg1 = arg2;
        }
        else if(arg1 == "nodes")
        {
            GetFirstArg(arg2, &arg1, &arg2);
            eng.time_base = INFINITY;
            eng.time_remains = eng.time_base;
            eng.max_nodes_to_search = atoi(arg1.c_str());
            eng.max_search_depth = k2chess::max_ply;

            arg1 = arg2;
        }
        else if(arg1 == "ponder")
        {
            eng.pondering_in_process = true;
            arg1 = arg2;
        }
        else
            break;

    }//while(true
    if(no_movestogo_arg)
        eng.moves_per_session = 0;
}





//--------------------------------
void EasyCommand(std::string in)
{
    UNUSED(in);
    pondering_enabled = false;
}





//--------------------------------
void HardCommand(std::string in)
{
    UNUSED(in);
    pondering_enabled = true;
}





//--------------------------------
void PonderhitCommand(std::string in)
{
    UNUSED(in);
    eng.PonderHit();
}





//--------------------------------
void MemoryCommand(std::string in)
{
    std::string arg1, arg2;
    arg1 = in;
    GetFirstArg(arg2, &arg1, &arg2);
    auto size_MB = atoi(arg1.c_str());
    eng.ReHash(size_MB);
}





//--------------------------------
void AnalyzeCommand(std::string in)
{
    UNUSED(in);
    force = false;
    eng.infinite_analyze = true;

#ifndef DONT_USE_THREAD_FOR_INPUT
    if(t.joinable())
        t.join();
    t = std::thread(&k2engine::MainSearch, &eng);
#else
    eng.MainSearch();
#endif // USE_THREAD_FOR_INPUT
}





//--------------------------------
void ExitCommand(std::string in)
{
    StopCommand(in);
    eng.infinite_analyze = false;
}





//--------------------------------
void SetvalueCommand(std::string in)
{
    std::string arg1, arg2;
    GetFirstArg(in, &arg1, &arg2);
    /*
        if(arg1 == "k_saf")
        {
            param.at(0) = atof(arg2.c_str());
        }

        else if(arg1 == "k_saf1")
        {
            param.at(1) = atof(arg2.c_str());
        }

        else
        {
            std::cout << "error: wrong parameter name" << std ::endl
                         << "resign" << std::endl;
            param.clear();
        }
    */
    eng.InitEngine();
}
