/*
 * \brief  Intel 500 PCH Tigerlake GPIO driver
 * \author Alexander Boettcher
 * \date   2023-07-13
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* local includes */
#include <common_types.h>

namespace Pio_driver {

	using namespace Pin_driver;

	struct Community;
	struct Gpp;
	struct Pin_id;
}


struct Pio_driver::Gpp
{
	enum Value {
		A = 0,  B =  1, C =  2, D = 3, E = 4, F = 5, H = 7,
		R = 17, S = 18, T = 19, U = 20
	} value;

	class Invalid : Exception { };

	static Gpp from_xml(Xml_node const &node)
	{
		typedef String<2> Name;
		Name name = node.attribute_value("gpp", Name());

		if (name == "A") return { A };
		if (name == "B") return { B };
		if (name == "C") return { C };
		if (name == "D") return { D };
		if (name == "E") return { E };
		if (name == "F") return { F };
		if (name == "H") return { H };
		if (name == "R") return { R };
		if (name == "S") return { S };
		if (name == "T") return { T };
		if (name == "U") return { U };

		warning("unknown gpp name '", name, "'");

		throw Invalid();
	}
};


struct Pio_driver::Community
{
	unsigned value;

	class Invalid : Exception { };

	static Community from_xml(Xml_node const &node)
	{
		auto const val = node.attribute_value("community", 9u);
		if (val != 3 && val < 6) return { val };

		warning("unknown community '", val, "'");

		throw Invalid();
	}
};


/**
 * Unique physical identifier of a pin
 */
struct Pio_driver::Pin_id
{
	Community community;
	Gpp       gpp;
	Index     index;

	static Pin_id from_xml(Xml_node const &node)
	{
		return { Community::from_xml(node), Gpp::from_xml(node),
		         Index::from_xml(node) };
	}

	bool operator == (Pin_id const &other) const
	{
		return other.community.value == community.value &&
		       other.gpp.value       == gpp.value       &&
		       other.index.value     == index.value;
	}

	bool operator != (Pin_id const &other) const {
		return !(operator == (other)); }

	void print(Output &out) const
	{
		if (community.value == 2)
			Genode::print(out, "community ", community.value, " - PAD_CFG_DW0_GPD",
			              "_", index.value);
		else
			Genode::print(out, "community ", community.value, " - PAD_CFG_DW0_GPPC",
			              Char('A' + (char)gpp.value), "_", index.value);
	}

	Pin_id(Community comm, Gpp gpp, Index index)
	: community(comm), gpp(gpp), index(index) { }
};

#endif /* _TYPES_H_ */
