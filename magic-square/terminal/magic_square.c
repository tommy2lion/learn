/*
 * Magic Square Game
 * A 3x3 puzzle: place numbers 1-9 so every row, column,
 * and diagonal sums to 15.
 *
 * Compile:  gcc magic_square.c -o magic_square
 * Run:      ./magic_square
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SIZE 3
#define MAGIC_SUM 15

/* All 8 valid 3x3 magic squares (rotations + reflections of the one base) */
static const int SOLUTIONS[8][SIZE][SIZE] = {
    {{2,7,6},{9,5,1},{4,3,8}},
    {{2,9,4},{7,5,3},{6,1,8}},
    {{4,9,2},{3,5,7},{8,1,6}},
    {{4,3,8},{9,5,1},{2,7,6}},
    {{6,1,8},{7,5,3},{2,9,4}},
    {{6,7,2},{1,5,9},{8,3,4}},
    {{8,1,6},{3,5,7},{4,9,2}},
    {{8,3,4},{1,5,9},{6,7,2}},
};

/* ── display ─────────────────────────────────────────────────────────────── */

void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void print_banner(void) {
    printf("\n");
    printf("  ╔══════════════════════════════╗\n");
    printf("  ║     ★  MAGIC SQUARE  ★       ║\n");
    printf("  ║  Fill the grid so every row, ║\n");
    printf("  ║  column & diagonal sums to   ║\n");
    printf("  ║           15                 ║\n");
    printf("  ╚══════════════════════════════╝\n\n");
}

void print_grid(const int grid[SIZE][SIZE], const int hidden[SIZE][SIZE]) {
    printf("  +-------+-------+-------+\n");
    for (int r = 0; r < SIZE; r++) {
        printf("  |");
        for (int c = 0; c < SIZE; c++) {
            if (hidden[r][c])
                printf("   ?   |");
            else
                printf("   %d   |", grid[r][c]);
        }
        printf("  (row %d)\n", r + 1);
        printf("  +-------+-------+-------+\n");
    }
    printf("     col1    col2    col3\n\n");
}

void print_grid_filled(const int grid[SIZE][SIZE]) {
    printf("  +-------+-------+-------+\n");
    for (int r = 0; r < SIZE; r++) {
        printf("  |");
        for (int c = 0; c < SIZE; c++)
            printf("   %d   |", grid[r][c]);
        printf("\n");
        printf("  +-------+-------+-------+\n");
    }
}

/* ── validation ──────────────────────────────────────────────────────────── */

int is_valid_magic(const int grid[SIZE][SIZE]) {
    int used[10] = {0};

    /* each number 1-9 used exactly once */
    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++) {
            int v = grid[r][c];
            if (v < 1 || v > 9 || used[v]) return 0;
            used[v] = 1;
        }

    /* rows */
    for (int r = 0; r < SIZE; r++) {
        int s = 0;
        for (int c = 0; c < SIZE; c++) s += grid[r][c];
        if (s != MAGIC_SUM) return 0;
    }

    /* columns */
    for (int c = 0; c < SIZE; c++) {
        int s = 0;
        for (int r = 0; r < SIZE; r++) s += grid[r][c];
        if (s != MAGIC_SUM) return 0;
    }

    /* diagonals */
    int d1 = 0, d2 = 0;
    for (int i = 0; i < SIZE; i++) {
        d1 += grid[i][i];
        d2 += grid[i][SIZE - 1 - i];
    }
    return (d1 == MAGIC_SUM && d2 == MAGIC_SUM);
}

/* ── puzzle generation ───────────────────────────────────────────────────── */

void make_puzzle(int grid[SIZE][SIZE], int hidden[SIZE][SIZE],
                 int *blanks, int hide_count) {
    /* pick a random valid solution */
    int sol = rand() % 8;
    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++) {
            grid[r][c]   = SOLUTIONS[sol][r][c];
            hidden[r][c] = 0;
        }

    /* hide exactly hide_count distinct cells */
    int positions[SIZE * SIZE];
    for (int i = 0; i < SIZE * SIZE; i++) positions[i] = i;

    /* Fisher-Yates shuffle first hide_count */
    for (int i = 0; i < hide_count; i++) {
        int j = i + rand() % (SIZE * SIZE - i);
        int tmp = positions[i]; positions[i] = positions[j]; positions[j] = tmp;
    }
    for (int i = 0; i < hide_count; i++) {
        int r = positions[i] / SIZE;
        int c = positions[i] % SIZE;
        hidden[r][c] = 1;
    }
    *blanks = hide_count;
}

/* ── hint ────────────────────────────────────────────────────────────────── */

void give_hint(const int grid[SIZE][SIZE], int hidden[SIZE][SIZE],
               int *blanks) {
    if (*blanks == 0) { printf("  No blanks left!\n"); return; }
    /* reveal a random hidden cell */
    int candidates[SIZE * SIZE], n = 0;
    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++)
            if (hidden[r][c]) candidates[n++] = r * SIZE + c;
    int pick = candidates[rand() % n];
    int r = pick / SIZE, c = pick % SIZE;
    hidden[r][c] = 0;
    (*blanks)--;
    printf("  Hint: position (row %d, col %d) = %d\n\n", r+1, c+1, grid[r][c]);
}

/* ── one round ───────────────────────────────────────────────────────────── */

/* Returns 1 if player solved it, 0 if they quit */
int play_round(int difficulty, int round_num) {
    int grid[SIZE][SIZE], hidden[SIZE][SIZE];
    int blanks;
    int hide_count = (difficulty == 1) ? 3 :
                     (difficulty == 2) ? 5 : 7;

    make_puzzle(grid, hidden, &blanks, hide_count);

    /* working copy the player fills in */
    int answer[SIZE][SIZE];
    for (int r = 0; r < SIZE; r++)
        for (int c = 0; c < SIZE; c++)
            answer[r][c] = grid[r][c];

    int hints_used = 0;
    int mistakes   = 0;

    while (blanks > 0) {
        clear_screen();
        print_banner();
        printf("  Round %d  |  Difficulty: %s  |  Blanks: %d  |  Hints used: %d\n\n",
               round_num,
               difficulty == 1 ? "Easy" : difficulty == 2 ? "Medium" : "Hard",
               blanks, hints_used);
        print_grid(answer, hidden);

        printf("  Commands:\n");
        printf("    <row> <col> <value>  — fill a cell  (e.g.  1 2 7)\n");
        printf("    h                    — get a hint   (-1 point)\n");
        printf("    q                    — quit game\n\n");
        printf("  > ");

        char line[64];
        if (!fgets(line, sizeof(line), stdin)) return 0;

        /* quit */
        if (line[0] == 'q' || line[0] == 'Q') return 0;

        /* hint */
        if (line[0] == 'h' || line[0] == 'H') {
            give_hint(grid, hidden, &blanks);
            /* sync answer with revealed hint */
            for (int r = 0; r < SIZE; r++)
                for (int c = 0; c < SIZE; c++)
                    if (!hidden[r][c]) answer[r][c] = grid[r][c];
            hints_used++;
            printf("  Press Enter to continue...");
            fgets(line, sizeof(line), stdin);
            continue;
        }

        /* fill a cell */
        int row, col, val;
        if (sscanf(line, "%d %d %d", &row, &col, &val) == 3) {
            row--; col--;   /* convert to 0-based */
            if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) {
                printf("  Invalid position. Row and col must be 1-3.\n");
                printf("  Press Enter to continue...");
                fgets(line, sizeof(line), stdin);
                continue;
            }
            if (!hidden[row][col]) {
                printf("  That cell is already filled in.\n");
                printf("  Press Enter to continue...");
                fgets(line, sizeof(line), stdin);
                continue;
            }
            if (val < 1 || val > 9) {
                printf("  Value must be between 1 and 9.\n");
                printf("  Press Enter to continue...");
                fgets(line, sizeof(line), stdin);
                continue;
            }
            answer[row][col] = val;
            if (val == grid[row][col]) {
                hidden[row][col] = 0;
                blanks--;
                printf("  Correct!\n");
            } else {
                answer[row][col] = 0;   /* reset wrong entry */
                mistakes++;
                printf("  Wrong! Try again. (Mistakes: %d)\n", mistakes);
            }
            printf("  Press Enter to continue...");
            fgets(line, sizeof(line), stdin);
        } else {
            printf("  Unrecognised input. Try:  1 2 7\n");
            printf("  Press Enter to continue...");
            fgets(line, sizeof(line), stdin);
        }
    }

    /* solved! */
    clear_screen();
    print_banner();
    printf("  *** CONGRATULATIONS! Magic Square solved! ***\n\n");
    print_grid_filled(answer);
    printf("\n  Mistakes: %d  |  Hints used: %d\n", mistakes, hints_used);

    /* verify once more just for display */
    if (is_valid_magic(answer))
        printf("  All rows, columns and diagonals sum to 15. Perfect!\n\n");

    printf("  Press Enter to continue...");
    char tmp[8];
    fgets(tmp, sizeof(tmp), stdin);
    return 1;
}

/* ── main menu ───────────────────────────────────────────────────────────── */

int main(void) {
    srand((unsigned)time(NULL));

    int difficulty = 2;  /* default: medium */
    int rounds_won = 0;
    int round_num  = 1;

    while (1) {
        clear_screen();
        print_banner();
        printf("  Score: %d rounds won\n\n", rounds_won);
        printf("  1. Play  (difficulty: %s)\n",
               difficulty == 1 ? "Easy" :
               difficulty == 2 ? "Medium" : "Hard");
        printf("  2. Change difficulty\n");
        printf("  3. How to play\n");
        printf("  4. Quit\n\n");
        printf("  > ");

        char line[32];
        if (!fgets(line, sizeof(line), stdin)) break;

        switch (line[0]) {
        case '1': {
            int won = play_round(difficulty, round_num++);
            if (won) rounds_won++;
            break;
        }
        case '2':
            clear_screen();
            print_banner();
            printf("  Select difficulty:\n");
            printf("  1. Easy   (3 blanks)\n");
            printf("  2. Medium (5 blanks)\n");
            printf("  3. Hard   (7 blanks)\n\n");
            printf("  > ");
            if (fgets(line, sizeof(line), stdin))
                if (line[0] >= '1' && line[0] <= '3')
                    difficulty = line[0] - '0';
            break;
        case '3':
            clear_screen();
            print_banner();
            printf("  HOW TO PLAY\n");
            printf("  -----------\n");
            printf("  A Magic Square is a 3x3 grid filled with numbers 1-9\n");
            printf("  (each used exactly once) such that the sum of every\n");
            printf("  row, every column, and both main diagonals equals 15.\n\n");
            printf("  Some cells are shown; your job is to fill in the rest.\n\n");
            printf("  To place a number, type:  <row> <col> <value>\n");
            printf("  Example:  2 3 7   places 7 in row 2, column 3.\n\n");
            printf("  Type 'h' for a hint (costs 1 hint point).\n");
            printf("  Type 'q' to quit to the main menu.\n\n");
            printf("  Press Enter to go back...");
            fgets(line, sizeof(line), stdin);
            break;
        case '4':
        case 'q':
        case 'Q':
            clear_screen();
            printf("  Thanks for playing! Final score: %d rounds won.\n\n",
                   rounds_won);
            return 0;
        }
    }
    return 0;
}
