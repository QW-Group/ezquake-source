#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NETWORK_CASES 64
#define MAX_NETWORK_RESULTS 64
#define MAX_HITREG_RESULTS 24
#define MAX_GOLDEN_ENTRIES 128
#define LINE_BUFFER 1024

#define MVD_PEXT1_ANTILAG_REASON_DISABLED      (1u << 0)
#define MVD_PEXT1_ANTILAG_REASON_ENABLED       (1u << 1)
#define MVD_PEXT1_ANTILAG_REASON_PING_REJECT   (1u << 2)
#define MVD_PEXT1_ANTILAG_REASON_CMD_FALLBACK  (1u << 3)
#define MVD_PEXT1_ANTILAG_REASON_CLAMP_MAXUNLAG (1u << 4)
#define MVD_PEXT1_ANTILAG_REASON_CLAMP_FUTURE  (1u << 5)

#define ANTILAG_POSITION_DISCONTINUITY (1u << 0)
#define ANTILAG_POSITION_ALIVE         (1u << 1)

typedef enum {
	resolve_none = 0,
	resolve_single,
	resolve_oldest,
	resolve_newest,
	resolve_discontinuity,
	resolve_interpolate,
	resolve_bad_interval
} resolve_mode_t;

typedef struct {
	char name[64];
	double sv_time;
	double frame_sv_time;
	double ping_ms;
	int net_drop;
	int last_msec;
	int oldest_msec;
	int old_msec;
	int new_msec;
	double sv_maxfps;
	double max_cmd_delta;
	double max_unlag;
	int no_pred;
} network_case_t;

typedef struct {
	char name[64];
	double target_time;
	unsigned int reason_flags;
} network_result_t;

typedef struct {
	double localtime;
	double origin[3];
	unsigned int flags;
} hitreg_sample_t;

typedef struct {
	char name[64];
	resolve_mode_t mode;
	double origin[3];
} hitreg_result_t;

typedef struct {
	char kind[16];
	char name[64];
	double target_time;
	unsigned int reason_flags;
	char mode[32];
	double origin[3];
} golden_entry_t;

static int ieq(const char *a, const char *b);

static int network_name_exists(const network_case_t *cases, int count, const char *name)
{
	int i;

	for (i = 0; i < count; ++i) {
		if (ieq(cases[i].name, name)) {
			return 1;
		}
	}
	return 0;
}

static int golden_name_exists(const golden_entry_t *entries, int count, const char *kind, const char *name)
{
	int i;

	for (i = 0; i < count; ++i) {
		if (ieq(entries[i].kind, kind) && ieq(entries[i].name, name)) {
			return 1;
		}
	}
	return 0;
}

static double clampd(double minv, double v, double maxv)
{
	if (v < minv) {
		return minv;
	}
	if (v > maxv) {
		return maxv;
	}
	return v;
}

static double maxd(double a, double b)
{
	return a > b ? a : b;
}

static double mind(double a, double b)
{
	return a < b ? a : b;
}

static int ieq(const char *a, const char *b)
{
	for (; *a && *b; ++a, ++b) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
			return 0;
		}
	}
	return (*a == '\0' && *b == '\0');
}

static const char *mode_to_string(resolve_mode_t mode)
{
	switch (mode) {
	case resolve_single:
		return "single";
	case resolve_oldest:
		return "oldest";
	case resolve_newest:
		return "newest";
	case resolve_discontinuity:
		return "discontinuity";
	case resolve_interpolate:
		return "interpolate";
	case resolve_bad_interval:
		return "bad_interval";
	default:
		return "none";
	}
}

static int parse_csv_line(char *line, char *fields[], int max_fields)
{
	int count = 0;
	char *cursor = line;

	while (*cursor && count < max_fields) {
		char *start;

		while (*cursor == ' ' || *cursor == '\t') {
			++cursor;
		}
		start = cursor;
		while (*cursor && *cursor != ',') {
			++cursor;
		}
		if (*cursor == ',') {
			*cursor = '\0';
			++cursor;
		}
		while (*start && isspace((unsigned char)start[strlen(start) - 1])) {
			start[strlen(start) - 1] = '\0';
		}
		fields[count++] = start;
	}

	return count;
}

static int load_network_cases(const char *path, network_case_t *cases, int max_cases)
{
	FILE *f;
	char line[LINE_BUFFER];
	int count = 0;

	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "phase9: failed to open network scenario file %s (%s)\n", path, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), f)) {
		char *fields[16];
		int nfields;
		network_case_t *out;

		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
			continue;
		}

		line[strcspn(line, "\r\n")] = '\0';
		nfields = parse_csv_line(line, fields, 16);
		if (nfields != 13) {
			fprintf(stderr, "phase9: invalid network scenario row (need 13 fields): %s\n", line);
			fclose(f);
			return -1;
		}
		if (!fields[0][0]) {
			fprintf(stderr, "phase9: network scenario name cannot be empty\n");
			fclose(f);
			return -1;
		}
		if (count >= max_cases) {
			fprintf(stderr, "phase9: too many network scenarios (max %d)\n", max_cases);
			fclose(f);
			return -1;
		}
		if (network_name_exists(cases, count, fields[0])) {
			fprintf(stderr, "phase9: duplicate network scenario name: %s\n", fields[0]);
			fclose(f);
			return -1;
		}

		out = &cases[count++];
		memset(out, 0, sizeof(*out));
		strncpy(out->name, fields[0], sizeof(out->name) - 1);
		out->sv_time = atof(fields[1]);
		out->frame_sv_time = atof(fields[2]);
		out->ping_ms = atof(fields[3]);
		out->net_drop = atoi(fields[4]);
		out->last_msec = atoi(fields[5]);
		out->oldest_msec = atoi(fields[6]);
		out->old_msec = atoi(fields[7]);
		out->new_msec = atoi(fields[8]);
		out->sv_maxfps = atof(fields[9]);
		out->max_cmd_delta = atof(fields[10]);
		out->max_unlag = atof(fields[11]);
		out->no_pred = atoi(fields[12]);
	}

	fclose(f);
	return count;
}

static int load_golden_entries(const char *path, golden_entry_t *entries, int max_entries)
{
	FILE *f;
	char line[LINE_BUFFER];
	int count = 0;

	f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "phase9: failed to open golden file %s (%s)\n", path, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), f)) {
		char *fields[8];
		int nfields;
		golden_entry_t *out;

		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
			continue;
		}

		line[strcspn(line, "\r\n")] = '\0';
		nfields = parse_csv_line(line, fields, 8);
		if (nfields < 4) {
			fprintf(stderr, "phase9: invalid golden row: %s\n", line);
			fclose(f);
			return -1;
		}
		if (count >= max_entries) {
			fprintf(stderr, "phase9: too many golden rows (max %d)\n", max_entries);
			fclose(f);
			return -1;
		}

		out = &entries[count++];
		memset(out, 0, sizeof(*out));
		strncpy(out->kind, fields[0], sizeof(out->kind) - 1);
		strncpy(out->name, fields[1], sizeof(out->name) - 1);
		if (!out->kind[0] || !out->name[0]) {
			fprintf(stderr, "phase9: golden row requires non-empty kind and name: %s\n", line);
			fclose(f);
			return -1;
		}
		if (golden_name_exists(entries, count - 1, out->kind, out->name)) {
			fprintf(stderr, "phase9: duplicate golden row for %s/%s\n", out->kind, out->name);
			fclose(f);
			return -1;
		}

		if (ieq(out->kind, "network")) {
			if (nfields != 4) {
				fprintf(stderr, "phase9: network golden row must have 4 fields: %s\n", line);
				fclose(f);
				return -1;
			}
			out->target_time = atof(fields[2]);
			out->reason_flags = (unsigned int)strtoul(fields[3], NULL, 10);
		}
		else if (ieq(out->kind, "hitreg")) {
			if (nfields != 6) {
				fprintf(stderr, "phase9: hitreg golden row must have 6 fields: %s\n", line);
				fclose(f);
				return -1;
			}
			strncpy(out->mode, fields[2], sizeof(out->mode) - 1);
			out->origin[0] = atof(fields[3]);
			out->origin[1] = atof(fields[4]);
			out->origin[2] = atof(fields[5]);
		}
		else {
			fprintf(stderr, "phase9: unknown golden kind %s\n", out->kind);
			fclose(f);
			return -1;
		}
	}

	fclose(f);
	return count;
}

static double safe_cmd_msec(int msec)
{
	return clampd(0.0, (double)msec, 50.0);
}

static double command_window_seconds(const network_case_t *c)
{
	double command_window = 0.0;
	int net_drop = c->net_drop;

	if (net_drop < 20 && net_drop > 2) {
		command_window += (net_drop - 2) * safe_cmd_msec(c->last_msec);
	}
	if (net_drop > 1) {
		command_window += safe_cmd_msec(c->oldest_msec);
	}
	if (net_drop > 0) {
		command_window += safe_cmd_msec(c->old_msec);
	}
	command_window += safe_cmd_msec(c->new_msec);

	return command_window * 0.001;
}

static double interpolation_delay(const network_case_t *c)
{
	const double max_prediction = 0.02;
	double max_physfps = c->sv_maxfps;
	double cmd_interval = safe_cmd_msec(c->new_msec) * 0.001;

	if (c->no_pred) {
		return 0.0;
	}

	if (max_physfps < 20.0 || max_physfps > 1000.0) {
		max_physfps = 77.0;
	}
	if (cmd_interval <= 0) {
		cmd_interval = 1.0 / max_physfps;
	}

	return mind(max_prediction, maxd(1.0 / max_physfps, cmd_interval));
}

static void compute_network_result(const network_case_t *c, network_result_t *out)
{
	double rtt = maxd(0.0, c->ping_ms * 0.001);
	double one_way = rtt * 0.5;
	double interp = interpolation_delay(c);
	double command_window = command_window_seconds(c);
	double command_time = c->frame_sv_time + command_window;
	double command_target = command_time - interp;
	double latency_target = c->sv_time - one_way - interp;
	double target_time = command_target;
	unsigned int reason_flags = 0;

	if (command_time > c->sv_time) {
		command_time = c->sv_time;
		command_target = command_time - interp;
	}

	if (c->max_cmd_delta > 0.0 && fabs(command_target - latency_target) > c->max_cmd_delta) {
		target_time = latency_target;
		reason_flags |= MVD_PEXT1_ANTILAG_REASON_CMD_FALLBACK;
	}

	if (c->max_unlag > 0.0 && target_time < c->sv_time - c->max_unlag) {
		target_time = c->sv_time - c->max_unlag;
		reason_flags |= MVD_PEXT1_ANTILAG_REASON_CLAMP_MAXUNLAG;
	}
	if (target_time > c->sv_time) {
		target_time = c->sv_time;
		reason_flags |= MVD_PEXT1_ANTILAG_REASON_CLAMP_FUTURE;
	}

	memset(out, 0, sizeof(*out));
	strncpy(out->name, c->name, sizeof(out->name) - 1);
	out->target_time = target_time;
	out->reason_flags = reason_flags;
}

static int resolve_position(const hitreg_sample_t *samples, int count, double target_time, double out_origin[3], resolve_mode_t *mode)
{
	const hitreg_sample_t *base;
	const hitreg_sample_t *interp;
	const hitreg_sample_t *oldest;
	const hitreg_sample_t *newest;
	const hitreg_sample_t *previous;
	int i;
	double span;
	double factor;

	if (!samples || count <= 0) {
		return 0;
	}

	oldest = &samples[0];
	newest = &samples[count - 1];
	base = oldest;
	interp = oldest;
	if (mode) {
		*mode = resolve_none;
	}

	if (count == 1 || target_time <= oldest->localtime) {
		base = interp = oldest;
	}
	else if (target_time >= newest->localtime) {
		base = interp = newest;
	}
	else {
		previous = oldest;
		for (i = 1; i < count; ++i) {
			const hitreg_sample_t *current = &samples[i];

			if (target_time <= current->localtime) {
				if ((previous->flags | current->flags) & ANTILAG_POSITION_DISCONTINUITY) {
					if (fabs(target_time - previous->localtime) <= fabs(current->localtime - target_time)) {
						base = interp = previous;
					}
					else {
						base = interp = current;
					}
				}
				else {
					base = previous;
					interp = current;
				}
				break;
			}
			previous = current;
		}
	}

	if (base == interp) {
		out_origin[0] = base->origin[0];
		out_origin[1] = base->origin[1];
		out_origin[2] = base->origin[2];
		if (mode) {
			if (count == 1) {
				*mode = resolve_single;
			}
			else if (base == oldest) {
				*mode = resolve_oldest;
			}
			else if (base == newest) {
				*mode = resolve_newest;
			}
			else {
				*mode = resolve_discontinuity;
			}
		}
		return 1;
	}

	span = interp->localtime - base->localtime;
	if (span <= 0.0 || isnan(span)) {
		if (fabs(target_time - base->localtime) <= fabs(interp->localtime - target_time)) {
			out_origin[0] = base->origin[0];
			out_origin[1] = base->origin[1];
			out_origin[2] = base->origin[2];
		}
		else {
			out_origin[0] = interp->origin[0];
			out_origin[1] = interp->origin[1];
			out_origin[2] = interp->origin[2];
		}
		if (mode) {
			*mode = resolve_bad_interval;
		}
		return 1;
	}

	factor = (target_time - base->localtime) / span;
	if (isnan(factor)) {
		factor = 0.0;
	}
	factor = clampd(0.0, factor, 1.0);
	out_origin[0] = base->origin[0] + (interp->origin[0] - base->origin[0]) * factor;
	out_origin[1] = base->origin[1] + (interp->origin[1] - base->origin[1]) * factor;
	out_origin[2] = base->origin[2] + (interp->origin[2] - base->origin[2]) * factor;
	if (mode) {
		*mode = resolve_interpolate;
	}
	return 1;
}

static int run_hitreg_scenarios(hitreg_result_t *out, int max_out)
{
	hitreg_sample_t samples[4];
	double resolved[3];
	resolve_mode_t mode = resolve_none;
	int count = 0;

	if (max_out < 8) {
		return -1;
	}

	memset(samples, 0, sizeof(samples));
	samples[0].localtime = 10.000; samples[0].origin[0] = 0;  samples[0].flags = ANTILAG_POSITION_ALIVE;
	samples[1].localtime = 10.050; samples[1].origin[0] = 32; samples[1].flags = ANTILAG_POSITION_ALIVE;
	samples[2].localtime = 10.100; samples[2].origin[0] = 64; samples[2].flags = ANTILAG_POSITION_ALIVE;
	resolve_position(samples, 3, 10.075, resolved, &mode);
	strncpy(out[count].name, "peek_shot", sizeof(out[count].name) - 1);
	out[count].mode = mode;
	out[count].origin[0] = resolved[0];
	out[count].origin[1] = resolved[1];
	out[count].origin[2] = resolved[2];
	count++;

	memset(samples, 0, sizeof(samples));
	samples[0].localtime = 20.000; samples[0].origin[0] = -80; samples[0].flags = ANTILAG_POSITION_ALIVE;
	samples[1].localtime = 20.030; samples[1].origin[0] = -20; samples[1].flags = ANTILAG_POSITION_ALIVE;
	samples[2].localtime = 20.060; samples[2].origin[0] = 20;  samples[2].flags = ANTILAG_POSITION_ALIVE;
	samples[3].localtime = 20.090; samples[3].origin[0] = 80;  samples[3].flags = ANTILAG_POSITION_ALIVE;
	resolve_position(samples, 4, 20.045, resolved, &mode);
	strncpy(out[count].name, "crossing_strafe", sizeof(out[count].name) - 1);
	out[count].mode = mode;
	out[count].origin[0] = resolved[0];
	out[count].origin[1] = resolved[1];
	out[count].origin[2] = resolved[2];
	count++;

	memset(samples, 0, sizeof(samples));
	samples[0].localtime = 30.000; samples[0].origin[0] = 0;  samples[0].flags = ANTILAG_POSITION_ALIVE;
	samples[1].localtime = 30.008; samples[1].origin[0] = 8;  samples[1].flags = ANTILAG_POSITION_ALIVE;
	samples[2].localtime = 30.041; samples[2].origin[0] = 41; samples[2].flags = ANTILAG_POSITION_ALIVE;
	samples[3].localtime = 30.043; samples[3].origin[0] = 43; samples[3].flags = ANTILAG_POSITION_ALIVE;
	resolve_position(samples, 4, 30.024, resolved, &mode);
	strncpy(out[count].name, "jitter_target", sizeof(out[count].name) - 1);
	out[count].mode = mode;
	out[count].origin[0] = resolved[0];
	out[count].origin[1] = resolved[1];
	out[count].origin[2] = resolved[2];
	count++;

	memset(samples, 0, sizeof(samples));
	samples[0].localtime = 31.000; samples[0].origin[0] = 0;  samples[0].flags = ANTILAG_POSITION_ALIVE;
	samples[1].localtime = 31.050; samples[1].origin[0] = 50; samples[1].flags = ANTILAG_POSITION_ALIVE | ANTILAG_POSITION_DISCONTINUITY;
	resolve_position(samples, 2, 31.030, resolved, &mode);
	strncpy(out[count].name, "discontinuity_snap", sizeof(out[count].name) - 1);
	out[count].mode = mode;
	out[count].origin[0] = resolved[0];
	out[count].origin[1] = resolved[1];
	out[count].origin[2] = resolved[2];
	count++;

	memset(samples, 0, sizeof(samples));
	samples[0].localtime = 31.000; samples[0].origin[0] = 0;  samples[0].flags = ANTILAG_POSITION_ALIVE;
	samples[1].localtime = 31.050; samples[1].origin[0] = 50; samples[1].flags = ANTILAG_POSITION_ALIVE | ANTILAG_POSITION_DISCONTINUITY;
	resolve_position(samples, 2, 31.010, resolved, &mode);
	strncpy(out[count].name, "discontinuity_snap_oldest", sizeof(out[count].name) - 1);
	out[count].mode = mode;
	out[count].origin[0] = resolved[0];
	out[count].origin[1] = resolved[1];
	out[count].origin[2] = resolved[2];
	count++;

	memset(samples, 0, sizeof(samples));
	samples[0].localtime = 40.000; samples[0].origin[0] = -10; samples[0].flags = ANTILAG_POSITION_ALIVE;
	samples[1].localtime = 40.050; samples[1].origin[0] = 20;  samples[1].flags = ANTILAG_POSITION_ALIVE;
	resolve_position(samples, 2, 39.500, resolved, &mode);
	strncpy(out[count].name, "out_of_window_oldest", sizeof(out[count].name) - 1);
	out[count].mode = mode;
	out[count].origin[0] = resolved[0];
	out[count].origin[1] = resolved[1];
	out[count].origin[2] = resolved[2];
	count++;

	memset(samples, 0, sizeof(samples));
	samples[0].localtime = 41.000; samples[0].origin[0] = -5; samples[0].flags = ANTILAG_POSITION_ALIVE;
	samples[1].localtime = 41.050; samples[1].origin[0] = 55; samples[1].flags = ANTILAG_POSITION_ALIVE;
	resolve_position(samples, 2, 41.500, resolved, &mode);
	strncpy(out[count].name, "out_of_window_newest", sizeof(out[count].name) - 1);
	out[count].mode = mode;
	out[count].origin[0] = resolved[0];
	out[count].origin[1] = resolved[1];
	out[count].origin[2] = resolved[2];
	count++;

	memset(samples, 0, sizeof(samples));
	samples[0].localtime = 42.000; samples[0].origin[0] = 123; samples[0].origin[1] = 4; samples[0].origin[2] = -7; samples[0].flags = ANTILAG_POSITION_ALIVE;
	resolve_position(samples, 1, 42.500, resolved, &mode);
	strncpy(out[count].name, "single_sample", sizeof(out[count].name) - 1);
	out[count].mode = mode;
	out[count].origin[0] = resolved[0];
	out[count].origin[1] = resolved[1];
	out[count].origin[2] = resolved[2];
	count++;

	return count;
}

static int find_golden_index(const golden_entry_t *entries, int count, const char *kind, const char *name)
{
	int i;

	for (i = 0; i < count; ++i) {
		if (ieq(entries[i].kind, kind) && ieq(entries[i].name, name)) {
			return i;
		}
	}
	return -1;
}

int main(int argc, char **argv)
{
	const char *network_path = NULL;
	const char *golden_path = NULL;
	network_case_t network_cases[MAX_NETWORK_CASES];
	network_result_t network_results[MAX_NETWORK_RESULTS];
	hitreg_result_t hitreg_results[MAX_HITREG_RESULTS];
	golden_entry_t golden_entries[MAX_GOLDEN_ENTRIES];
	int golden_matched[MAX_GOLDEN_ENTRIES];
	int network_case_count;
	int hitreg_count;
	int golden_count;
	int failures = 0;
	int i;

	for (i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--network") && i + 1 < argc) {
			network_path = argv[++i];
		}
		else if (!strcmp(argv[i], "--golden") && i + 1 < argc) {
			golden_path = argv[++i];
		}
		else {
			fprintf(stderr, "usage: %s --network <path> --golden <path>\n", argv[0]);
			return 2;
		}
	}

	if (!network_path || !golden_path) {
		fprintf(stderr, "usage: %s --network <path> --golden <path>\n", argv[0]);
		return 2;
	}

	network_case_count = load_network_cases(network_path, network_cases, MAX_NETWORK_CASES);
	if (network_case_count < 0) {
		return 2;
	}
	if (network_case_count > MAX_NETWORK_RESULTS) {
		fprintf(stderr, "phase9: too many network cases for result buffer\n");
		return 2;
	}

	for (i = 0; i < network_case_count; ++i) {
		compute_network_result(&network_cases[i], &network_results[i]);
	}

	hitreg_count = run_hitreg_scenarios(hitreg_results, MAX_HITREG_RESULTS);
	if (hitreg_count < 0) {
		fprintf(stderr, "phase9: failed to build hitreg scenarios\n");
		return 2;
	}

	golden_count = load_golden_entries(golden_path, golden_entries, MAX_GOLDEN_ENTRIES);
	if (golden_count < 0) {
		return 2;
	}
	memset(golden_matched, 0, sizeof(golden_matched));

	for (i = 0; i < network_case_count; ++i) {
		int g_index = find_golden_index(golden_entries, golden_count, "network", network_results[i].name);
		const golden_entry_t *g = (g_index >= 0 ? &golden_entries[g_index] : NULL);

		if (!g) {
			fprintf(stderr, "phase9: missing golden network entry for %s\n", network_results[i].name);
			failures++;
			continue;
		}
		golden_matched[g_index] = 1;

		if (fabs(network_results[i].target_time - g->target_time) > 0.0005 ||
			network_results[i].reason_flags != g->reason_flags) {
			fprintf(stderr, "phase9: network mismatch %s got(target=%0.6f flags=%u) expected(target=%0.6f flags=%u)\n",
				network_results[i].name,
				network_results[i].target_time,
				network_results[i].reason_flags,
				g->target_time,
				g->reason_flags);
			failures++;
		}
		else {
			printf("phase9: network ok %s target=%0.6f flags=%u\n",
				network_results[i].name, network_results[i].target_time, network_results[i].reason_flags);
		}
	}

	for (i = 0; i < hitreg_count; ++i) {
		int g_index = find_golden_index(golden_entries, golden_count, "hitreg", hitreg_results[i].name);
		const golden_entry_t *g = (g_index >= 0 ? &golden_entries[g_index] : NULL);
		const char *mode = mode_to_string(hitreg_results[i].mode);

		if (!g) {
			fprintf(stderr, "phase9: missing golden hitreg entry for %s\n", hitreg_results[i].name);
			failures++;
			continue;
		}
		golden_matched[g_index] = 1;

		if (!ieq(mode, g->mode)
			|| fabs(hitreg_results[i].origin[0] - g->origin[0]) > 0.05
			|| fabs(hitreg_results[i].origin[1] - g->origin[1]) > 0.05
			|| fabs(hitreg_results[i].origin[2] - g->origin[2]) > 0.05) {
			fprintf(stderr, "phase9: hitreg mismatch %s got(mode=%s origin=%0.2f %0.2f %0.2f) expected(mode=%s origin=%0.2f %0.2f %0.2f)\n",
				hitreg_results[i].name,
				mode,
				hitreg_results[i].origin[0], hitreg_results[i].origin[1], hitreg_results[i].origin[2],
				g->mode,
				g->origin[0], g->origin[1], g->origin[2]);
			failures++;
		}
		else {
			printf("phase9: hitreg ok %s mode=%s origin=%0.2f %0.2f %0.2f\n",
				hitreg_results[i].name,
				mode,
				hitreg_results[i].origin[0], hitreg_results[i].origin[1], hitreg_results[i].origin[2]);
		}
	}

	for (i = 0; i < golden_count; ++i) {
		if (!golden_matched[i] && (ieq(golden_entries[i].kind, "network") || ieq(golden_entries[i].kind, "hitreg"))) {
			fprintf(stderr, "phase9: unmatched golden entry %s/%s\n", golden_entries[i].kind, golden_entries[i].name);
			failures++;
		}
	}

	if (failures) {
		fprintf(stderr, "PHASE9_REGRESSION_FAIL %d\n", failures);
		return 1;
	}

	printf("PHASE9_REGRESSION_PASS network=%d hitreg=%d\n", network_case_count, hitreg_count);
	return 0;
}
