#pragma once

namespace Events
{
	// Registers the game event sinks that drive immediate re-application
	// (3D/facegen assembled, RaceMenu opened/closed, item equipped).
	// These are standard BSTEventSink registrations -- no function hooks.
	void Install();
}
