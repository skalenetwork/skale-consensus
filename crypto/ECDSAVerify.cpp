//
// Created by kladko on 20.02.20.
//




#include <gmp.h>
#include <stdbool.h>
#include "SkaleCommon.h"
#include "Log.h"
#include "ECDSAVerify.h"

/*Verify the integrity of a message using it's signature*/





/*Set a point from another point*/
void point_set(point R, point P)
{
    //Copy the point
    mpz_set(R->x, P->x);
    mpz_set(R->y, P->y);
    //Including infinity settings
    R->infinity = P->infinity;
}


/*Make R a copy of P*/
void point_copy(point R, point P)
{
    //Same as point set
    point_set(R, P);
}


/*Release point*/
void point_clear(point p)
{
    mpz_clear(p->x);
    mpz_clear(p->y);
    free(p);
}

/*Set point to be a infinity*/
void point_at_infinity(point p)
{
    p->infinity = true;
}

/*Initialize a point*/
point point_init()
{
    point p;
    p = (point) calloc(sizeof(struct point_s), 1);
    mpz_init(p->x);
    mpz_init(p->y);
    p->infinity = false;
    return p;
}



void number_theory_inverse(mpz_t R, mpz_t A, mpz_t P) {
    mpz_invert(R, A, P);
}

void number_theory_exp_modp_ui(mpz_t R, mpz_t a, unsigned long int k, mpz_t P) {
    //Do this using gmp number theory implementation
    mpz_powm_ui(R, a, k, P);
}


/*Set point R = 2P*/
void point_doubling(point R, point P, domain_parameters curve)
{
    //If at infinity
    if(P->infinity)
    {
        R->infinity = true;
    }else{
        //Initialize slope variable

        //Initialize temporary variables
        mpz_t s, t1, t2, t3, t4, t5;
        mpz_init(s); mpz_init(t1); mpz_init(t2); mpz_init(t3); mpz_init(t4); mpz_init(t5);

        //Calculate slope
        //s = (3*Px² + a) / (2*Py) mod p
        number_theory_exp_modp_ui(t1, P->x, 2, curve->p);	//t1 = Px² mod p
        mpz_mul_ui(t2, t1, 3);				//t2 = 3 * t1
        mpz_mod(t3, t2, curve->p);			//t3 = t2 mod p
        mpz_add(t4, t3, curve->a);			//t4 = t3 + a
        mpz_mod(t5, t4, curve->p);			//t5 = t4 mod p

        mpz_mul_ui(t1, P->y, 2);			//t1 = 2*Py
        number_theory_inverse(t2, t1, curve->p);		//t2 = t1^-1 mod p
        mpz_mul(t1, t5, t2);				//t1 = t5 * t2
        mpz_mod(s, t1, curve->p);			//s = t1 mod p

        //Calculate Rx
        //Rx = s² - 2*Px mod p
        number_theory_exp_modp_ui(t1, s, 2, curve->p);//t1 = s² mod p
        mpz_mul_ui(t2, P->x, 2);		//t2 = Px*2
        mpz_mod(t3, t2, curve->p);		//t3 = t2 mod p
        mpz_sub(t4, t1, t3);			//t4 = t1 - t3
        mpz_mod(R->x, t4, curve->p);	//Rx = t4 mod p

        //Calculate Ry using algorithm shown to the right of the commands
        //Ry = s(Px-Rx) - Py mod p
        mpz_sub(t1, P->x, R->x);			//t1 = Px - Rx
        mpz_mul(t2, s, t1);					//t2 = s*t1
        mpz_sub(t3, t2, P->y);				//t3 = t2 - Py
        mpz_mod(R->y, t3, curve->p);	//Ry = t3 mod p

        //Clear variables, release memory
        mpz_clear(t1); mpz_clear(t2); mpz_clear(t3); mpz_clear(t4); mpz_clear(t5); mpz_clear(s);
    }
}

/*Compare two points return 1 if not the same, returns 0 if they are the same*/
bool point_cmp(point P, point Q)
{
    //If at infinity
    if(P->infinity && Q->infinity)
        return true;
    else if(P->infinity || Q->infinity)
        return false;
    else
        return !mpz_cmp(P->x,Q->x) && !mpz_cmp(P->y,Q->y);
}


/*Set R to the additive inverse of P, in the curve curve*/
void point_inverse(point R, point P, domain_parameters curve)
{
    //If at infinity
    if(P->infinity)
    {
        R->infinity = true;
    }else{
        //Set Rx = Px
        mpz_set(R->x, P->x);

        //Set Ry = -Py mod p = p - Ry (Since, Ry < p and Ry is positive)
        mpz_sub(R->y, curve->p, P->y);
    }
}

/*Addition of point P + Q = result*/
void point_addition(point result, point P, point Q, domain_parameters curve)
{
    //If Q is at infinity, set result to P
    if(Q->infinity)
    {
        point_set(result, P);

        //If P is at infinity set result to be Q
    }else if(P->infinity){
        point_set(result, Q);

        //If the points are the same use point doubling
    }else if(point_cmp(P,Q))
    {
        point_doubling(result, Q, curve);
    }else{
        //Calculate the inverse point
        point iQ = point_init();
        point_inverse(iQ, Q, curve);
        bool is_inverse = point_cmp(iQ,P);
        point_clear(iQ);

        //If it is the inverse
        if(is_inverse)
        {
            //result must be point at infinity
            point_at_infinity(result);
        }else{
            //Initialize slope variable
            mpz_t s;mpz_init(s);
            //Initialize temporary variables
            mpz_t t1, t2, t3, t4, t5 ;
            mpz_init(t1); mpz_init(t2); mpz_init(t3); mpz_init(t4);  mpz_init(t5);
            /*
            Modulo algebra rules:
            (b1 + b2) mod  n = (b2 mod n) + (b1 mod n) mod n
            (b1 * b2) mod  n = (b2 mod n) * (b1 mod n) mod n
            */

            //Calculate slope
            //s = (Py - Qy)/(Px-Qx) mod p
            mpz_sub(t1, P->y, Q->y);
            mpz_sub(t2, P->x, Q->x);
            //Using Modulo to stay within the group!
            number_theory_inverse(t3, t2, curve->p); //Handle errors
            mpz_mul(t4, t1, t3);
            mpz_mod(s, t4, curve->p);

            //Calculate Rx using algorithm shown to the right of the commands
            //Rx = s² - Px - Qx = (s² mod p) - (Px mod p) - (Qx mod p) mod p
            number_theory_exp_modp_ui(t1, s, 2, curve->p);	//t1 = s² mod p
            mpz_mod(t2, P->x, curve->p);		//t2 = Px mod p
            mpz_mod(t3, Q->x, curve->p);		//t3 = Qx mod p
            mpz_sub(t4, t1, t2);				//t4 = t1 - t2
            mpz_sub(t5, t4, t3);				//t5 = t4 - t3
            mpz_mod(result->x, t5, curve->p);	//R->x = t5 mod p

            //Calculate Ry using algorithm shown to the right of the commands
            //Ry = s(Px-Rx) - Py mod p
            mpz_sub(t1, P->x, result->x);		//t1 = Px - Rx
            mpz_mul(t2, s, t1);					//t2 = s*t1
            mpz_sub(t3, t2, P->y);				//t3 = t2 - Py
            mpz_mod(result->y, t3, curve->p);	//Ry = t3 mod p

            //Clear variables, release memory
            mpz_clear(t1);mpz_clear(t2); mpz_clear(t3); mpz_clear(t4); mpz_clear(t5); mpz_clear(s);
        }
    }
}

/*Perform scalar multiplication to P, with the factor multiplier, over the curve curve*/
void point_multiplication(point R, mpz_t multiplier, point P, domain_parameters curve)
{
    //If at infinity R is also at infinity
    if(P->infinity)
    {
        R->infinity = true;
    }else{
        //Initializing variables
        point x = point_init();
        point_copy(x, P);
        point t = point_init();
        point_copy(t, x);

        //Set R = point at infinity
        point_at_infinity(R);

/*
Loops through the integer bit per bit, if a bit is 1 then x is added to the result. Looping through the multiplier in this manner allows us to use as many point doubling operations as possible. No reason to say 5P=P+P+P+P+P, when you might as well just use 5P=2(2P)+P.
This is not the most effecient method of point multiplication, but it's faster than P+P+P+... which is not computational feasiable.
*/
        uint64_t bits = mpz_sizeinbase(multiplier, 2);
        uint64_t bit = 0;
        while(bit <= bits)
        {
            if(mpz_tstbit(multiplier, bit))
            {
                point_addition(t, x, R, curve);
                point_copy(R, t);
            }
            point_doubling(t, x, curve);
            point_copy(x, t);
            bit++;
        }

        //Release temporary variables
        point_clear(x);
        point_clear(t);
    }
}

bool ECDSAVerify::signature_verify(mpz_t message, signature sig, point public_key, domain_parameters curve) {

    //Initialize variables
    mpz_t one, w, u1, u2, t, tt2;
    mpz_init(one); mpz_init(w); mpz_init(u1);
    mpz_init(u2); mpz_init(t); mpz_init(tt2);

    mpz_set_ui(one, 1);

    point x = point_init();
    point t1 = point_init();
    point t2 = point_init();

    bool result = false;


    if (mpz_cmp(sig->r, one) < 0 &&
        mpz_cmp(curve->n, sig->r) <= 0 &&
        mpz_cmp(sig->s, one) < 0 &&
        mpz_cmp(curve->n, sig->s) <= 0) {
        goto clean;
    }

    //w = s¯¹ mod n
    number_theory_inverse(w, sig->s, curve->n);

    //u1 = message * w mod n
    mpz_mod(tt2, message, curve->n);
    mpz_mul(t, tt2, w);
    mpz_mod(u1, t, curve->n);

    //u2 = r*w mod n
    mpz_mul(t, sig->r, w);
    mpz_mod(u2, t, curve->n);

    //x = u1*G+u2*Q
    point_multiplication(t1, u1, curve->G, curve);
    point_multiplication(t2, u2, public_key, curve);
    point_addition(x, t1, t2, curve);

    //Get the result, by comparing x value with r and verifying that x is NOT at infinity

    result = mpz_cmp(sig->r, x->x) == 0 && !x->infinity;


    clean:


    point_clear(x);
    point_clear(t1);
    point_clear(t2);

    mpz_clear(one); mpz_clear(w); mpz_clear(u1); mpz_clear(u2); mpz_clear(t);
    mpz_clear(tt2);

    return result;
}