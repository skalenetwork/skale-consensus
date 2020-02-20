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


    domain_parameters curve;

    static void point_set(point R, point P);
    static void point_copy(point R, point P);
    static void point_clear(point p);
    static void point_at_infinity(point p);
    static point point_init();
    static void number_theory_inverse(mpz_t R, mpz_t A, mpz_t P);
    static void number_theory_exp_modp_ui(mpz_t R, mpz_t a, unsigned long int k, mpz_t P);
    static void point_doubling(point R, point P, domain_parameters curve);
    static bool point_cmp(point P, point Q);
    static void point_inverse(point R, point P, domain_parameters curve);
    static void point_addition(point result, point P, point Q, domain_parameters curve);
    static void point_multiplication(point R, mpz_t multiplier, point P, domain_parameters curve);
    static void domain_parameters_set_name(domain_parameters curve, char* name);
    static void point_set_str(point p, char *x, char *y, int base);
    static void point_set_hex(point p, char *x, char *y);
    static void domain_parameters_set_hex(
            domain_parameters curve, char* name, char* p, char* a, char* b, char* Gx, char* Gy, char* n, char* h);
    static void domain_parameters_load_curve(domain_parameters out);

        /*Initialize a curve*/
    domain_parameters domain_parameters_init();

public:

    ECDSAVerify();
    
    bool signature_verify(ptr<SHAHash> hash, signature sig, ptr<string> publicKeyHex);
};


#endif //SKALED_ECDSAVERIFY_H
