export module gse.network:discovery;

import std;

import :socket;

import gse.math;
import gse.utility;

export namespace gse::network {
    struct discovery_result {
        address addr;
        std::string name;
        std::string map;
        std::uint8_t players{};
        std::uint8_t max_players{};
        std::uint32_t build{};
    };

    struct discovery_provider {
    	virtual ~discovery_provider() = default;

        virtual auto refresh(
            time_t<std::uint32_t>
        ) -> void = 0;

        virtual auto results(
	    ) const -> std::span<const discovery_result> = 0;
    };

    class wan_directory_provider : public discovery_provider {
    public:
        wan_directory_provider(
            std::vector<discovery_result> seed = {}
        );

        auto refresh(
            time_t<std::uint32_t>
        ) -> void override;

        auto results(
        ) const -> std::span<const discovery_result> override;

        auto set_seed(
            std::vector<discovery_result> seed
        ) -> void ;
    private:
        std::vector<discovery_result> m_results;
        time_t<std::uint32_t> m_last_refresh{ seconds(0) };
    };
}

gse::network::wan_directory_provider::wan_directory_provider(std::vector<gse::network::discovery_result> seed) : m_results(std::move(seed)) {}

auto gse::network::wan_directory_provider::refresh(time_t<std::uint32_t>) -> void {
    m_last_refresh = system_clock::now();
}

auto gse::network::wan_directory_provider::results() const -> std::span<const discovery_result> {
    return m_results;
}

auto gse::network::wan_directory_provider::set_seed(std::vector<discovery_result> seed) -> void {
	m_results = std::move(seed);
}

