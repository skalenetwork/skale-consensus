//
// Created by kladko on 20.02.20.
//

#ifndef SKALED_ECDSAVERIFY_H
#define SKALED_ECDSAVERIFY_H


typedef struct point_s* point;
struct point_s
{
    mpz_t x;
    mpz_t y;
    bool infinity;
};

/*Type that represents a curve*/
typedef struct domain_parameters_s* domain_parameters;
struct domain_parameters_s
{
    char* name;
    mpz_t p;	//Prime
    mpz_t a;	//'a' parameter of the elliptic curve
    mpz_t b;	//'b' parameter of the elliptic curve
    point G;	//Generator point of the curve, also known as base point.
    mpz_t n;
    mpz_t h;
};


/*Type for representing a signature*/
struct signature_s
{
    mpz_t r;
    mpz_t s;
    unsigned int v;
};

typedef struct signature_s* signature;

class ECDSAVerify {
    static bool signature_verify(mpz_t message, signature sig, point public_key, domain_parameters curve);
};


#endif //SKALED_ECDSAVERIFY_H
