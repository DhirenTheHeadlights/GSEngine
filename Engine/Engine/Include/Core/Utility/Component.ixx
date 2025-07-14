export module gse.utility:component;

import std;

import :id;

export namespace gse {
	struct component : identifiable_owned {
		explicit component(const id& owner_id) : identifiable_owned(owner_id) {}
	};
}
