#include "pch.h"
#include "CustomGameWrapper.h"

using namespace std::placeholders;

template <typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type*>
void CustomGameWrapper::HookEventWithCaller(EventName eventName, Identifier identifier, Callback<T> callback)
{
	HookEventGeneric(
		eventName,
		identifier,
		callback,
		std::bind(&GameWrapper::HookEventWithCaller<ActorWrapper>, gameWrapper, _1, _2),
		callbacks
	);
}
void CustomGameWrapper::HookEvent(EventName eventName, Identifier identifier, SimpleCallback callback)
{
	HookEventWithCaller<ActorWrapper>(
		eventName,
		identifier,
		[callback](ActorWrapper caller, void* params, std::string event) { callback(event); }
	);
}
void CustomGameWrapper::UnhookEvent(EventName eventName, Identifier identifier)
{
	UnhookEventGeneric(eventName, identifier, std::bind(&GameWrapper::UnhookEvent, gameWrapper, _1), callbacks);
}

template <typename T, typename std::enable_if<std::is_base_of<ObjectWrapper, T>::value>::type*>
void CustomGameWrapper::HookEventWithCallerPost(EventName eventName, Identifier identifier, Callback<T> callback)
{
	HookEventGeneric(
		eventName,
		identifier,
		callback,
		std::bind(&GameWrapper::HookEventWithCallerPost<ActorWrapper>, gameWrapper, _1, _2),
		callbacksPost
	);
}
void CustomGameWrapper::HookEventPost(EventName eventName, Identifier identifier, SimpleCallback callback)
{
	HookEventWithCallerPost<ActorWrapper>(
		eventName,
		identifier,
		[callback](ActorWrapper caller, void* params, std::string event) { callback(event); }
	);
}
void CustomGameWrapper::UnhookEventPost(EventName eventName, Identifier identifier)
{
	UnhookEventGeneric(eventName, identifier, std::bind(&GameWrapper::UnhookEventPost, gameWrapper, _1), callbacksPost);
}

void CustomGameWrapper::RegisterDrawable(Identifier identifier, CallbackDraw callback)
{
	if (callbacksDraw.empty())
	{
		gameWrapper->RegisterDrawable(
			[this](CanvasWrapper canvas)
			{
				ExecuteCallbacks(callbacksDraw, canvas);
			});
	}

	if (callbacksDraw.contains(identifier))
		LOG("Draw callback with identifier {} already exists... Will be overwritten", identifier);

	callbacksDraw[identifier] = callback;
}
void CustomGameWrapper::UnregisterDrawables(Identifier identifier)
{
	callbacksDraw.erase(identifier);

	if (callbacksDraw.empty())
	{
		gameWrapper->UnregisterDrawables();
	}
}

template <typename ... Args>
void CustomGameWrapper::ExecuteCallbacks(std::map<Identifier, std::function<void(Args...)>>& callbackMap, Args... args)
{
	// We need to collect all identifiers beforehand, 
	// because we could unhook callbacks inside a callback.
	std::vector<Identifier> identifiers;
	for (auto& [identifier, callback] : callbackMap)
		identifiers.push_back(identifier);

	for (auto& identifier : identifiers)
	{
		if (!callbackMap.contains(identifier))
			continue;

		callbackMap[identifier](args...);
	}
}

template<typename T, typename Function>
void CustomGameWrapper::HookEventGeneric(EventName eventName, Identifier identifier, Callback<T> callback, Function hook, auto& callbackMap)
{
	if (callbackMap[eventName].empty())
	{
		hook(eventName,
			[this, eventName, &callbackMap](ActorWrapper caller, void* params, std::string event)
			{
				ExecuteCallbacks(callbackMap[eventName], caller, params, event);
			});
	}

	if (callbackMap[eventName].contains(identifier))
		LOG("Callback for event {} with identifier {} already exists... Will be overwritten", eventName, identifier);

	callbackMap[eventName][identifier] = [callback](ActorWrapper caller, void* params, std::string event)
		{
			callback(T(caller.memory_address), params, event);
		};
}

template<typename Function>
void CustomGameWrapper::UnhookEventGeneric(EventName eventName, Identifier identifier, Function unhook, auto& callbackMap)
{
	callbackMap[eventName].erase(identifier);

	if (callbackMap[eventName].empty())
	{
		unhook(eventName);
	}
}
