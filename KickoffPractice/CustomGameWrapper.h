#pragma once

#include "bakkesmod/wrappers/GameWrapper.h"

using EventName = std::string;
using Identifier = std::string;
using SimpleCallback = std::function<void(EventName eventName)>;
template <typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type* = nullptr>
using Callback = std::function<void(T caller, void* params, EventName eventName)>;
using CallbackDraw = std::function<void(CanvasWrapper)>;

// Allows hooking into the same event multiple times.
// By default `GameWrapper` only executes the first callback to the same event 
// (even if hooked via `HookEvent()` and `HookEventWithCaller()`).
// Using `HookEvent()` and `HookEventPost()` at the same time is fine though.
class CustomGameWrapper
{
public:
	CustomGameWrapper(std::shared_ptr<GameWrapper> gameWrapper) : gameWrapper(gameWrapper) {}

	template <typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type* = nullptr>
	void HookEventWithCaller(EventName eventName, Identifier identifier, Callback<T> callback);
	void HookEvent(EventName eventName, Identifier identifier, SimpleCallback callback);
	void UnhookEvent(EventName eventName, Identifier identifier);

	template <typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type* = nullptr>
	void HookEventWithCallerPost(EventName eventName, Identifier identifier, Callback<T> callback);
	void HookEventPost(EventName eventName, Identifier identifier, SimpleCallback callback);
	void UnhookEventPost(EventName eventName, Identifier identifier);

	void RegisterDrawable(Identifier identifier, CallbackDraw callback);
	void UnregisterDrawables(Identifier identifier);

private:
	std::shared_ptr<GameWrapper>gameWrapper;

	std::map<EventName, std::map<Identifier, Callback<ActorWrapper>>> callbacks;
	std::map<EventName, std::map<Identifier, Callback<ActorWrapper>>> callbacksPost;
	std::map<Identifier, CallbackDraw> callbacksDraw;

	template <typename ... Args>
	void ExecuteCallbacks(std::map<Identifier, std::function<void(Args...)>>& callbackMap, Args... args);

	template <typename T, typename Function>
	void HookEventGeneric(EventName eventName, Identifier identifier, Callback<T> callback, Function hook, auto& callbackMap);

	template <typename Function>
	void UnhookEventGeneric(EventName eventName, Identifier identifier, Function unhook, auto& callbackMap);
};