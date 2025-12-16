export module gse.graphics:clip;

import std;

import :skeleton;

export namespace gse {
	struct joint_keyframe {
        time time;
        mat4 local_transform;
    };

    struct joint_track {
        std::uint16_t joint_index;
        std::vector<joint_keyframe> keys;
    };

    class clip_asset : identifiable {
    public:
        struct params {
            const std::string& name;
            float length;
            bool loop;
            std::vector<joint_track> tracks;
        };

        explicit clip_asset(
			params params
        );
    private:
        params m_params;
    };
}

gse::clip_asset::clip_asset(params params) : identifiable(params.name), m_params(std::move(params)) {}
