namespace semantic_contract {
	struct record {
		int member = 0;
	};

	enum class choice {
		value,
	};

	int global = 0;

	[[nodiscard]] auto function(record& parameter) -> int {
		int variable = parameter.member + global;
		return variable + parameter.member + static_cast<int>(choice::value);
	}
}

auto result = semantic_contract::function(*static_cast<semantic_contract::record*>(nullptr));

struct callable {
	auto method(int parameter) const -> int {
		return parameter;
	}
};

auto call_member(callable& value) -> int {
	return value.method(1);
}
