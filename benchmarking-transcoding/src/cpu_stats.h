#ifndef CPU_STATS_H
#define CPU_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Estrutura para armazenar estatísticas de CPU lidas de /proc/stat
 * Todos os valores são em "jiffies" (unidades de tempo do kernel, tipicamente 1/100s)
 */
typedef struct {
    unsigned long long user;       // Tempo em modo usuário
    unsigned long long nice;       // Tempo em modo usuário com prioridade baixa (nice)
    unsigned long long system;     // Tempo em modo kernel
    unsigned long long idle;       // Tempo idle
    unsigned long long iowait;     // Tempo esperando I/O completar
    unsigned long long irq;        // Tempo servindo interrupções
    unsigned long long softirq;    // Tempo servindo softirqs
    unsigned long long steal;      // Tempo roubado por outras instâncias virtualizadas
    unsigned long long guest;      // Tempo rodando guest virtualizado
    unsigned long long guest_nice; // Tempo rodando guest virtualizado com nice
} CpuStats;

/**
 * Lê estatísticas de CPU agregadas de /proc/stat
 *
 * @param stats Ponteiro para estrutura CpuStats a ser preenchida
 * @return 0 em sucesso, -1 em erro
 */
int cpu_stats_read(CpuStats *stats);

/**
 * Calcula o uso de CPU como porcentagem entre duas leituras
 *
 * @param prev Leitura anterior de CPU
 * @param curr Leitura atual de CPU
 * @return Porcentagem de uso de CPU (0.0 a 100.0), ou -1.0 em erro
 */
double cpu_stats_calculate_usage(const CpuStats *prev, const CpuStats *curr);

#ifdef __cplusplus
}
#endif

#endif /* CPU_STATS_H */
