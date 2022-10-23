#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#define N_GRADES 101
#define OPT_SUMS_SZ 128

typedef int pref_sum_t[OPT_SUMS_SZ];

void fill_random_vector(int *mat, int n) {
    for (int i = 0; i < n; i++)
        mat[i] = rand() % N_GRADES;
}

static inline void pref_sum(int *sums) {
    for (int i = 1; i < OPT_SUMS_SZ; i++)
        sums[i] += sums[i-1];
}

static inline void counting_sort_accum_freq(const int *restrict mat, int *restrict sums, size_t mat_len) {
    for (int i = 0; i < mat_len; i++)
        sums[mat[i]]++;

    pref_sum(sums);
}

static inline void counting_sort_freq(const int *restrict mat, int *restrict freq, size_t mat_len) {
    for (int i = 0; i < mat_len; i++)
        freq[mat[i]]++;
}

const int* lower_bound_128(const int *first, int value) {
    first += 64 * (first[63] < value);
    first += 32 * (first[31] < value);
    first += 16 * (first[15] < value);
    first +=  8 * (first[ 7] < value);
    first +=  4 * (first[ 3] < value);
    first +=  2 * (first[ 1] < value);
    first +=  1 * (first[ 0] < value);
    first +=  1 * (first[ 0] < value);
    return first;
}

// Source: https://en.cppreference.com/w/cpp/algorithm/upper_bound
const int* upper_bound(const int* first, size_t n, int value) {
    const int* it;
    size_t step;

    while (n > 0) {
        it = first;
        step = n / 2;
        it += step;
        if (value >= *it) {
            first = ++it;
            n -= step + 1;
        }
        else
            n = step;
    }
    return first;
}

static inline double median_from_sums_128(const pref_sum_t sums) {
    int count = sums[127];
    if (count % 2 == 0) {
        const int median_up = lower_bound_128(sums, count/2) - sums;
        const int median_down = lower_bound_128(sums, (count/2)+1) - sums;
        return (double)(median_down + median_up) / 2.0;
    } else {
        const int median = lower_bound_128(sums, count/2) - sums;
        return (double)median;
    }
}

static inline void compute_statistics_from_sums(const int *restrict sums, int *restrict min,
                                                int *restrict max, double *restrict median,
                                                double *restrict mean, double *restrict stdev) {
    const int n_occur = sums[N_GRADES-1];
    int total = 0;
    int64_t total_sq = 0;
    for (int i = 1; i < N_GRADES; i++) {
        int count = sums[i] - sums[i-1];
        total += count * i;
        total_sq += count * i * i;
    }

    *min = upper_bound(sums, N_GRADES, 0) - sums;
    *max = lower_bound_128(sums, n_occur) - sums;
    *median = median_from_sums_128(sums);
    const double m = (double)total / (double)n_occur;
    *mean = m;
    *stdev = sqrt((double)total_sq / (double)n_occur - m * m);
}

void compute_all_statistics(int *mat, int r, int c, int a,
                            // City
                            int *restrict min_city, int *restrict max_city,
                            double *restrict median_city, double *restrict mean_city,
                            double *restrict stdev_city,
                            // Region
                            int *restrict min_reg, int *restrict max_reg,
                            double *restrict median_reg, double *restrict mean_reg,
                            double *restrict stdev_reg,
                            // Brasil
                            int *restrict min_total, int *restrict max_total,
                            double *restrict median_total, double *restrict mean_total,
                            double *restrict stdev_total, int *restrict best_reg,
                            int *restrict best_city_reg, int *restrict best_city) {
    const size_t ngrades_per_region = c * a;

    double best_reg_mean = 0, best_city_mean = 0;

    pref_sum_t sums_total;
    __builtin_memset(sums_total, 0, sizeof(sums_total));
    for (int reg = 0; reg < r; reg++) {
        pref_sum_t sums_reg;
        __builtin_memset(sums_reg, 0, sizeof(sums_reg));

        for (int city = 0; city < c; city++) {
            const int i = reg * ngrades_per_region + city * a;
            const int j = reg * c + city;

            pref_sum_t sums;
            __builtin_memset(sums, 0, sizeof(sums));
            counting_sort_accum_freq(mat + i, sums, a);

            compute_statistics_from_sums(sums, &min_city[j], &max_city[j],
                                         &median_city[j], &mean_city[j], &stdev_city[j]);

            if (best_city_mean < mean_city[j]) {
                best_city_mean = mean_city[j];
                *best_city_reg = reg;
                *best_city = city;
            }

            for (int i = 0; i < OPT_SUMS_SZ; i++)
                sums_reg[i] += sums[i];
        }

        compute_statistics_from_sums(sums_reg, &min_reg[reg], &max_reg[reg],
                                     &median_reg[reg], &mean_reg[reg], &stdev_reg[reg]);

        if (best_reg_mean < median_reg[reg]) {
            best_reg_mean = median_reg[reg];
            *best_reg = reg;
        }

        for (int i = 0; i < OPT_SUMS_SZ; i++)
            sums_total[i] += sums_reg[i];
    }

    compute_statistics_from_sums(sums_total, min_total, max_total,
                                 median_total, mean_total, stdev_total);
}

int main(int argc, char *argv[]) {
    // Leitura dos dados de entrada
    int r, c, a, seed;
#ifndef PERF
    if (!scanf("%d %d %d %d", &r, &c, &a, &seed)) return 1;
#else
    if (argc < 5) return 1;
    r = atoi(argv[1]);
    c = atoi(argv[2]);
    a = atoi(argv[3]);
    seed = atoi(argv[4]);
#endif

    const size_t n = r * c * a;
    const size_t ncity = r * c;
    const size_t ngrades_per_region = c * a;
    int* mat = (int*)malloc(n * sizeof(int));
    // City
    int* min_city = (int*)malloc(ncity * sizeof(int));
    int* max_city = (int*)malloc(ncity * sizeof(int));
    double* median_city = (double*)malloc(ncity * sizeof(double));
    double* mean_city = (double*)malloc(ncity * sizeof(double));
    double* stdev_city = (double*)malloc(ncity * sizeof(double));

    // Region
    int* min_reg = (int*)malloc(r * sizeof(int));
    int* max_reg = (int*)malloc(r * sizeof(int));
    double* median_reg = (double*)malloc(r * sizeof(double));
    double* mean_reg = (double*)malloc(r * sizeof(double));
    double* stdev_reg = (double*)malloc(r * sizeof(double));

    // Brasil
    int best_reg, best_city_reg, best_city;
    int min_total, max_total;
    double median_total, mean_total, stdev_total;

    srand(seed);
    fill_random_vector(mat, n);

#ifdef DEBUG
    for (int reg = 0; reg < r; reg++) {
        printf("Região %d\n", reg);
        for (int city = 0; city < c; city++) {
            for (int grade = 0; grade < a; grade++) {
                const int i = reg * ngrades_per_region + city * a + grade;
                printf("%d ", mat[i]);
            }
            printf("\n");
        }
        printf("\n");
    }
#endif

    clock_t start_clock = clock();
    compute_all_statistics(mat, r, c, a, min_city, max_city, median_city, mean_city, stdev_city,
                                min_reg, max_reg, median_reg, mean_reg, stdev_reg,
                                &min_total, &max_total, &median_total, &mean_total, &stdev_total, &best_reg,
                                &best_city_reg, &best_city);
    clock_t time_taken = clock() - start_clock;

#ifndef PERF
    for (int reg = 0; reg < r; reg++) {
        for (int city = 0; city < c; city++) {
            int i = reg * c + city;
            printf(
                "Reg %d - Cid %d: menor: %d, maior: %d, mediana: %.02lf, média: %.02lf e DP: %.02lf\n",
                reg, city, min_city[i], max_city[i], median_city[i], mean_city[i], stdev_city[i]
            );
        }
        printf("\n");
    }

    for (int reg = 0; reg < r; reg++) {
        printf(
            "Reg %d: menor: %d, maior: %d, mediana: %.02lf, média: %.02lf e DP: %.02lf\n",
            reg, max_reg[reg], min_reg[reg], median_reg[reg], mean_reg[reg], stdev_reg[reg]
        );
    }
    printf("\n");

    printf(
        "Brasil: menor: %d, maior: %d, mediana: %.02lf, média: %.02lf e DP: %.02lf\n",
        min_total, max_total, median_total, mean_total, stdev_total
    );
    printf("\n");

    printf("Melhor região: Região %d\n", best_reg);
    printf("Melhor cidade: Região %d, Cidade: %d\n", best_city_reg, best_city);
    printf("\n");
#else
    // Use variables to prevent them beeing optimized out

    // City
    *(volatile int*)min_city;
    *(volatile int*)max_city;
    *(volatile double*)median_city;
    *(volatile double*)mean_city;
    *(volatile double*)stdev_city;

    // Region
    *(volatile int*)min_reg;
    *(volatile int*)max_reg;
    *(volatile double*)median_reg;
    *(volatile double*)mean_reg;
    *(volatile double*)stdev_reg,

    // Brasil
    *(volatile int*)mat;
    *(volatile int*)&min_total;
    *(volatile int*)&max_total;
    *(volatile double*)&median_total;
    *(volatile double*)&mean_total;
    *(volatile double*)&stdev_total;
    *(volatile int*)&best_reg;
    *(volatile int*)&best_city_reg;
    *(volatile int*)&best_city;
#endif

    printf("Tempo de resposta sem considerar E/S, em segundos: %.03lfs\n", (double)time_taken / CLOCKS_PER_SEC);

    free(mat);
    free(min_city);
    free(max_city);
    free(median_city);
    free(mean_city);
    free(stdev_city);
    free(min_reg);
    free(max_reg);
    free(median_reg);
    free(mean_reg);
    free(stdev_reg);

    return 0;
}
