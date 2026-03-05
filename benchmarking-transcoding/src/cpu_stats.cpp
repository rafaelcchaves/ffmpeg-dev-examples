/**
 * cpu_stats.cpp - Módulo para leitura de estatísticas de CPU via /proc/stat
 *
 * Lê a linha agregada "cpu " de /proc/stat para calcular uso de CPU global.
 *
 * NOTA: Esta medição é GLOBAL do sistema, não específica do processo.
 */

#include "cpu_stats.h"
#include <stdio.h>
#include <string.h>

int cpu_stats_read(CpuStats *stats) {
    if (!stats) {
        return -1;
    }

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return -1;
    }

    // Inicializa com zeros
    memset(stats, 0, sizeof(CpuStats));

    // Lê a primeira linha que começa com "cpu " (agregada)
    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    // Parse da linha "cpu  user nice system idle iowait irq softirq steal guest guest_nice"
    // Os campos guest e guest_nice podem estar ausentes em kernels antigos
    int parsed = sscanf(line,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
        &stats->user,
        &stats->nice,
        &stats->system,
        &stats->idle,
        &stats->iowait,
        &stats->irq,
        &stats->softirq,
        &stats->steal,
        &stats->guest,
        &stats->guest_nice
    );

    // Precisa de pelo menos os 4 campos básicos (user, nice, system, idle)
    if (parsed < 4) {
        return -1;
    }

    return 0;
}

double cpu_stats_calculate_usage(const CpuStats *prev, const CpuStats *curr) {
    if (!prev || !curr) {
        return -1.0;
    }

    // Calcula totais
    unsigned long long prev_total = prev->user + prev->nice + prev->system +
                                    prev->idle + prev->iowait + prev->irq +
                                    prev->softirq + prev->steal + prev->guest +
                                    prev->guest_nice;

    unsigned long long curr_total = curr->user + curr->nice + curr->system +
                                    curr->idle + curr->iowait + curr->irq +
                                    curr->softirq + curr->steal + curr->guest +
                                    curr->guest_nice;

    // Tempo ativo = total - idle (idle inclui iowait como "não ativo")
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long curr_idle = curr->idle + curr->iowait;

    unsigned long long prev_active = prev_total - prev_idle;
    unsigned long long curr_active = curr_total - curr_idle;

    // Delta
    unsigned long long delta_total = curr_total - prev_total;
    unsigned long long delta_active = curr_active - prev_active;

    // Evita divisão por zero
    if (delta_total == 0) {
        return 0.0;
    }

    // Calcula porcentagem
    return 100.0 * (double)delta_active / (double)delta_total;
}
