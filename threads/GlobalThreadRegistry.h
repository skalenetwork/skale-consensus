//
// Created by kladko on 24.09.19.
//

#ifndef SKALED_GLOBALTHREADREGISTRY_H
#define SKALED_GLOBALTHREADREGISTRY_H


class GlobalThreadRegistry {


    static vector< thread*  > allThreads;
    static recursive_mutex mutex;
    static bool joined;

public:

    static void joinAll();

    static void add(thread* _t);


};


#endif //SKALED_GLOBALTHREADREGISTRY_H
