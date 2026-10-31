/* Host-only replay driver. The production model owns every selection/commit. */
#include "keyboard_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool pending_matches(const HelloKeyboardModel* model, char target) {
    char pending[HELLO_KEYBOARD_ITEM_BYTES + 1];
    hello_keyboard_model_pending_text(model, pending, sizeof(pending));
    return target == ' ' ? !strcmp(pending, "⎵") : pending[0] == target && !pending[1];
}

static void tap(HelloKeyboardModel* model, HelloKeyboardButton button, uint32_t* now) {
    hello_keyboard_model_press(model, button, ++*now);
    hello_keyboard_model_release(model, button, ++*now);
}

int main(void) {
    char text[HELLO_KEYBOARD_TEXT_BYTES + 2];
    while (fgets(text, sizeof(text), stdin)) {
        text[strcspn(text, "\r\n")] = 0;
        HelloKeyboardModel model;
        hello_keyboard_model_init(&model, "", 0);
        unsigned dpad = 0, buttons = 0;
        uint32_t now = 0;
        for (size_t at = 0; text[at]; ++at) {
            int best_direction = -1, best_button = -1, best_taps = 1000;
            /* Search by executing the real model, not by duplicating its layout
               or selection formula. Each character is a fresh group gesture. */
            for (int direction = HELLO_KB_UP; direction <= HELLO_KB_LEFT; ++direction) {
                for (int button = HELLO_KB_BUTTON_A; button <= HELLO_KB_BUTTON_B; ++button) {
                    HelloKeyboardModel trial = model;
                    uint32_t trial_now = now;
                    hello_keyboard_model_direction(&trial, (HelloKeyboardDirection)direction, ++trial_now);
                    const int limit = hello_keyboard_model_group_length(&trial, trial.group);
                    for (int count = 1; count <= limit; ++count) {
                        tap(&trial, (HelloKeyboardButton)button, &trial_now);
                        if (pending_matches(&trial, text[at])) {
                            if (count < best_taps) {
                                best_direction = direction; best_button = button; best_taps = count;
                            }
                            break;
                        }
                    }
                }
            }
            if (best_direction < 0) { fprintf(stderr, "Cannot type byte at %zu\n", at); return EXIT_FAILURE; }
            hello_keyboard_model_direction(&model, (HelloKeyboardDirection)best_direction, ++now);
            ++dpad;
            for (int count = 0; count < best_taps; ++count) tap(&model, (HelloKeyboardButton)best_button, &now);
            buttons += (unsigned)best_taps;
            hello_keyboard_model_direction_release(&model, (HelloKeyboardDirection)best_direction, ++now);
            /* Require release acceptance now, with no idle wait or forced commit. */
            if (model.group || model.pending || strlen(model.text) != at + 1 || strncmp(model.text, text, at + 1)) {
                fprintf(stderr, "Replay differs at byte %zu\n", at); return EXIT_FAILURE;
            }
        }
        if (strcmp(model.text, text)) return EXIT_FAILURE;
        printf("{\"characters\":%zu,\"dpad_presses\":%u,\"selection_presses\":%u,\"presses\":%u,\"releases\":%u,\"release_commits\":%u,\"replay_verified\":true}\n",
            strlen(text), dpad, buttons, dpad + buttons, dpad + buttons, dpad);
    }
    return ferror(stdin) ? EXIT_FAILURE : EXIT_SUCCESS;
}
