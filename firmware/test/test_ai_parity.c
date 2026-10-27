#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
            if (c_pred == dt_pred && confidence >= 0.0f && confidence <= 1.0f) {
                matched_samples++;
            } else {
                printf("MISMATCH at sample %d: C_pred=%d != Py_pred=%d (Features: %.2f, %.2f, %.2f, %.2f)\n",
                       total_samples, c_pred, dt_pred, soil, temp, hum, light);
            }
        }
    }
    fclose(fp);

    /* Boundary & Stress Assertions in C */
    int edge_tests_passed = 0;
    int total_edge_tests = 4;

    float conf = 0.0f;
    /* 1. Extreme high temperature fault */
    int pred = edge_ai_model_predict(45.0f, 85.0f, 40.0f, 60.0f, &conf);
    if (pred == AI_CLASS_SENSOR_FAULT) {
        edge_tests_passed++;
    } else {
        printf("Failed: Expected SENSOR_FAULT for temp 85C, got %d\n", pred);
    }

    /* 2. Extreme low temperature fault */
    pred = edge_ai_model_predict(45.0f, -20.0f, 40.0f, 60.0f, &conf);
    if (pred == AI_CLASS_SENSOR_FAULT) {
        edge_tests_passed++;
    } else {
        printf("Failed: Expected SENSOR_FAULT for temp -20C, got %d\n", pred);
    }

    /* 3. Class label strings */
    if (strcmp(edge_ai_model_class_to_str(AI_CLASS_NORMAL), "NORMAL") == 0 &&
        strcmp(edge_ai_model_class_to_str(AI_CLASS_NEED_WATER), "NEED_WATER") == 0) {
        edge_tests_passed++;
    }

    /* 4. Invalid class id handling */
    if (strcmp(edge_ai_model_class_to_str(-1), "UNKNOWN") == 0 &&
        strcmp(edge_ai_model_class_to_str(99), "UNKNOWN") == 0) {
        edge_tests_passed++;
    }

    printf("====================================================\n");
    printf("   EDGE AI C TRANSPILATION PARITY VERIFICATION     \n");
    printf("====================================================\n");
    printf("Total Samples Evaluated: %d\n", total_samples);
    printf("Matches with Python DT : %d\n", matched_samples);
    printf("Parity Match Rate      : %.2f%%\n", (float)matched_samples / total_samples * 100.0f);
    printf("Edge/Boundary Tests    : %d / %d passed\n", edge_tests_passed, total_edge_tests);
    printf("====================================================\n");

    return (matched_samples == total_samples && edge_tests_passed == total_edge_tests) ? 0 : 1;
}
