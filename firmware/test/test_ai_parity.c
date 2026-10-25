#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edge_ai_model.h"

int main(void) {
    FILE *fp = fopen("ml/export/test_samples.csv", "r");
    if (!fp) {
        printf("Error: Could not open ml/export/test_samples.csv\n");
        return 1;
    }

    char line[256];
    /* Skip CSV header */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 1;
    }

    int total_samples = 0;
    int matched_samples = 0;

    while (fgets(line, sizeof(line), fp)) {
        float soil, temp, hum, light;
        int true_class, dt_pred;
        if (sscanf(line, "%f,%f,%f,%f,%d,%d", &soil, &temp, &hum, &light, &true_class, &dt_pred) == 6) {
            float confidence = 0.0f;
            int c_pred = edge_ai_model_predict(soil, temp, hum, light, &confidence);
            total_samples++;
            if (c_pred == dt_pred) {
                matched_samples++;
            } else {
                printf("MISMATCH at sample %d: C_pred=%d != Py_pred=%d (Features: %.2f, %.2f, %.2f, %.2f)\n",
                       total_samples, c_pred, dt_pred, soil, temp, hum, light);
            }
        }
    }
    fclose(fp);

    printf("====================================================\n");
    printf("   EDGE AI C TRANSPILATION PARITY VERIFICATION     \n");
    printf("====================================================\n");
    printf("Total Samples Evaluated: %d\n", total_samples);
    printf("Matches with Python DT : %d\n", matched_samples);
    printf("Parity Match Rate      : %.2f%%\n", (float)matched_samples / total_samples * 100.0f);
    printf("====================================================\n");

    return (matched_samples == total_samples) ? 0 : 1;
}
