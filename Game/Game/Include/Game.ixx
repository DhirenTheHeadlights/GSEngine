export module Game;

export namespace Game {
	void setInputHandlingFlag(bool enabled);
	bool initialize();
	bool update();
	bool render();
	bool close();
}
