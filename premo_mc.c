#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define MAX_DECK_SIZE 64

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define DIV_ROUND_CLOSEST(x, divisor) 			\
({							                    \
	typeof(x) __x = x;				            \
	typeof(divisor) __d = divisor;			    \
	(((typeof(x))-1) > 0 ||				        \
	 ((typeof(divisor))-1) > 0 ||			    \
	 (((__x) > 0) == ((__d) > 0))) ?		    \
		(((__x) + ((__d) / 2)) / (__d)) :	    \
		(((__x) - ((__d) / 2)) / (__d));	    \
})

typedef struct
{
    /* List of cards. */
    unsigned *l;
    /* Size of deck. */
    unsigned n;
} deck_t;

typedef struct
{
    /* Score value. */
    unsigned val;
    /* Corresponding card. */
    unsigned card;
} score_t;

static uint32_t rn(void);
static uint32_t rn_ranged(uint32_t l);
static int score_comp(const void *a, const void *b);
static void new_deck(deck_t *d);
static int binomial(unsigned n);
static void riffle(deck_t *d);
static void score(deck_t *d, score_t *scl);
static void cut(deck_t *d);
static unsigned top_in(deck_t *d);

uint32_t rn(void)
{
    /* Guarantee a bit in the MSB and help with low-quality LSBs. */
    return (uint32_t)rand() ^ __builtin_bswap32((uint32_t)rand());
}

uint32_t rn_ranged(uint32_t l)
{
    uint32_t r;
    do
    {
        r = rn();
    }
    while (r > ((UINT32_MAX / l) * l));
    return r % l; 
}

int score_comp(const void *a, const void *b)
{
    int ret = 0;
    const score_t *sa = (const score_t *)a;
    const score_t *sb = (const score_t *)b;

    if (sa->val > sb->val)
    {
        ret = -1;
    }
    else if (sa->val < sb->val)
    {
        ret = 1;
    }

    return ret;
}

void new_deck(deck_t *d)
{
    unsigned idx;
    for (idx = 0; idx < d->n; idx++)
    {
        d->l[idx] = idx;
    }
}

int binomial(unsigned n)
{
    uint64_t r;

    assert(n <= 64);

    r = ((uint64_t)rn()) << 32;
    r |= rn();

    r <<= (64 - n);

    return __builtin_popcountl(r);
}

void riffle(deck_t *d)
{
    int k;
    unsigned n;
    unsigned *l;
    unsigned l0[MAX_DECK_SIZE];
    unsigned l1[MAX_DECK_SIZE];

    n = d->n;
    l = d->l;

    /* Cut according to binomial (aim for equal halves). */
    k = binomial(n);
    memcpy(l0, l, (k)*sizeof(l[0]));
    memcpy(l1, l + k, (n-k)*sizeof(l[0]));

    /* Riffle */
    {
        unsigned i = 0;
        unsigned i0 = 0;
        unsigned i1 = 0;
        uint32_t n0 = k;
        uint32_t n1 = n-k;
        unsigned v;

        /* If halves have size n0 and n1, then select from each half according
         * to:
         *
         * P(left hand)  = n0 / (n0 + n1);
         * P(right hand) = n1 / (n0 + n1);
         */
        do
        {
            uint32_t r = rn_ranged(n0 + n1);
            if (r < n0)
            {
                v = l0[i0++];
                n0--;
            }
            else
            {
                v = l1[i1++];
                n1--;
            }
            l[i++] = v;
        } while (i < n);
    }
}

/* For each card count the number of cards between the card preceding it (in
 * terms of face value) and the card succeeding it.
 *
 * For example, if our deck is 7 8 3 9 2 4, card 3 has score (4 + 3).
 */
void score(deck_t *d, score_t *scl)
{
    unsigned idx;
    unsigned n;
    unsigned *l;

    n = d->n;
    l = d->l;
    for (idx = 0; idx < n; idx++)
    {
        unsigned *pc, *nc;
        unsigned *cc, *ec;

        cc = &l[idx];
        ec = &l[idx];
        pc = NULL;
        nc = NULL;

        while (pc == NULL || nc == NULL)
        {
            cc++;
            if (cc == (l + n)) cc = l;
            if (cc == ec) break;
            if (*cc == ((*ec + 1) % n)) nc = cc;
            if (*cc == ((*ec + (n-1)) % n)) pc = cc;
        }
        assert(pc && nc);
        scl[idx].val = (((ec - pc) + n) % n) + (((nc - ec) + n) % n) - 1;
        scl[idx].card = *ec;
    }
}

/* Cut deck according to uniform distribution. */
void cut(deck_t *d)
{
    unsigned k;
    unsigned n;
    unsigned *l;
    unsigned lt[MAX_DECK_SIZE];

    n = d->n;
    l = d->l;

    assert(n <= MAX_DECK_SIZE);

    k = rn_ranged(n);

    memcpy(lt, l + k, (n-k)*sizeof(lt[0]));
    memcpy(lt + (n-k), l, (k)*sizeof(lt[0]));

    memcpy(l, lt, n*sizeof(lt[0]));
}

/* Take the top card and insert it randomly according to the binomial
 * distribution. */
unsigned top_in(deck_t *d)
{
    unsigned k;
    unsigned n;
    unsigned temp;
    unsigned *l;

    n = d->n;
    l = d->l;

    assert(n <= MAX_DECK_SIZE);

    k = binomial(n);

    temp = l[n-1];
    memmove(l+(k+1), l+k, (n-k-1)*sizeof(l[0]));
    l[k] = temp;

    return temp;
}

int main(void)
{
#define RUNS 1000000
#define GUESS_MAX 26
#define DECK_SIZE 52
    unsigned ridx;
    unsigned riffles[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 52};
    unsigned success[ARRAY_SIZE(riffles)][GUESS_MAX] = {0};

    srand(time(NULL));

    for (ridx = 0; ridx < ARRAY_SIZE(riffles); ridx++)
    {
        unsigned count;

        for (count = 0; count < RUNS; count++)
        {
            unsigned m;
            unsigned card;
            unsigned gidx;
            int done;
            unsigned l[DECK_SIZE];
            score_t scl[DECK_SIZE];
            deck_t d =
            {
                .l = l,
                .n = DECK_SIZE
            };

            m = riffles[ridx];

            /* The trick itself. */
            {
                new_deck(&d);
                while (m--) riffle(&d);
                cut(&d);
                card = top_in(&d);
                cut(&d);
            }

            score(&d, scl);
            qsort(scl, sizeof(scl)/sizeof(scl[0]), sizeof(scl[0]), score_comp);

            done = 0;
            for (gidx = 0; gidx < GUESS_MAX; gidx++)
            {
                if (done)
                {
                    success[ridx][gidx]++;
                }
                else
                {
                    if (scl[gidx].card == card)
                    {
                        success[ridx][gidx]++;
                        done = 1;
                    }
                }
            }
        }
    }

    /* Print results table. */
    {
        unsigned gidx;
        printf("%2c", 'm');
        for (ridx = 0; ridx < ARRAY_SIZE(riffles); ridx++)
        {
            printf("%5u", riffles[ridx]);
        }
        printf("\n");
        for (gidx = 0; gidx < GUESS_MAX; gidx++)
        {
            printf("%2u", gidx+1);
            for (ridx = 0; ridx < ARRAY_SIZE(riffles); ridx++)
            {
                unsigned val;
                unsigned res;
                val = success[ridx][gidx];
                res = DIV_ROUND_CLOSEST(val, 1000);
                printf("%5u", res);
            }
            printf("\n");
        }
    }

    return 0;
}
