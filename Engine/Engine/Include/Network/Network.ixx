export module gse.network;

export namespace gse::network {
	auto initialize_enet() -> void;
	//void update();
	inline auto close_enet() -> void {  }
	//void createMultiplayerSession();
	auto join_network_session() -> void;
	//void setOwnAddress(std::string IP, int port);
	//void setServerAddress(std::string IP, int port);
}
