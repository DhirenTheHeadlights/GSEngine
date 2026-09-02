groupshared uint s_cumulative[histogram_bins];
groupshared float s_selected[histogram_bins];
groupshared float s_weighted[histogram_bins];

float bin_log_luminance(uint bin) {
    float log_min = log2(histogram_min_luminance);
    float log_max = log2(histogram_max_luminance);
    float t = (float(bin - 1) + 0.5) / float(histogram_bins - 2);
    return log_min + t * (log_max - log_min);
}

void exposure_update(uint group_index) {
    uint bin = group_index;
    uint count = bin == 0 ? 0 : histogram_in[bin];
    histogram_in[bin] = 0;
    s_cumulative[bin] = count;
    GroupMemoryBarrierWithGroupSync();

    for (uint offset = 1; offset < histogram_bins; offset <<= 1) {
        uint addend = bin >= offset ? s_cumulative[bin - offset] : 0;
        GroupMemoryBarrierWithGroupSync();
        s_cumulative[bin] += addend;
        GroupMemoryBarrierWithGroupSync();
    }

    float total = float(s_cumulative[histogram_bins - 1]);
    float low = total * pc.low_percentile;
    float high = total * pc.high_percentile;
    float cumulative_end = float(s_cumulative[bin]);
    float cumulative_start = cumulative_end - float(count);
    float selected = max(min(cumulative_end, high) - max(cumulative_start, low), 0.0);
    s_selected[bin] = selected;
    s_weighted[bin] = bin == 0 ? 0.0 : selected * bin_log_luminance(bin);
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = histogram_bins / 2; stride > 0; stride >>= 1) {
        if (bin < stride) {
            s_selected[bin] += s_selected[bin + stride];
            s_weighted[bin] += s_weighted[bin + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (bin != 0) {
        return;
    }

    float previous = exposure_out[0];
    float histogram_target = previous;
    if (s_selected[0] > 0.0) {
        float average = exp2(s_weighted[0] / s_selected[0]);
        histogram_target = middle_grey / clamp(average, pc.min_luminance, pc.max_luminance) * pc.compensation;
    }
    float incident_target = middle_grey / clamp(pc.incident_luminance, pc.min_luminance, pc.max_luminance) * pc.compensation;
    float incident_weight = pc.min_luminance > 0.0 ? saturate(pc.incident_luminance / pc.min_luminance) : 0.0;
    float target = exp2(lerp(log2(histogram_target), log2(incident_target), incident_weight));
    float blend = target < previous ? pc.blend_bright : pc.blend_dark;
    exposure_out[0] = exp2(lerp(log2(previous), log2(target), blend));
}
