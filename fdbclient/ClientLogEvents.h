/*
 * ClientLogEvents.h
 *
 * This source file is part of the FoundationDB open source project
 *
 * Copyright 2013-2018 Apple Inc. and the FoundationDB project authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#ifndef FDBCLIENT_CLIENTLOGEVENTS_H
#define FDBCLIENT_CLIENTLOGEVENTS_H

#include <flow/Trace.h>
#include <flow/serialize_variant.h>

#include "FDBTypes.h"
#include "MasterProxyInterface.h"

namespace FdbClientLogEvents {
typedef int EventType;
enum {
	GET_VERSION_LATENCY = 0,
	GET_LATENCY = 1,
	GET_RANGE_LATENCY = 2,
	COMMIT_LATENCY = 3,
	ERROR_GET = 4,
	ERROR_GET_RANGE = 5,
	ERROR_COMMIT = 6,

	EVENTTYPEEND // End of EventType
};

struct Event {
	Event(EventType t, double ts) : type(t), startTs(ts) {}
	Event() {}

	template <typename Ar>
	void serialize(Ar& ar) {
		old_serializer(ar, type, startTs);
	}

	EventType type{ EVENTTYPEEND };
	double startTs{ 0 };

	void logEvent(std::string id) const {}

	// this is a helper method to make new serialization a bit prettier
	Event& base() { return *this; }

	const Event& base() const { return *this; }
};

struct EventGetVersion : public Event {
	EventGetVersion(double ts, double lat) : Event(GET_VERSION_LATENCY, ts), latency(lat) {}
	EventGetVersion() {}

	template <typename Ar>
	void serialize(Ar& ar) {
		if (!ar.isDeserializing) {
			old_serializer(ar, base(), latency);
		} else
			old_serializer(ar, latency);
	}

	double latency;

	void logEvent(std::string id) const {
		TraceEvent("TransactionTrace_GetVersion").detail("TransactionID", id).detail("Latency", latency);
	}
};

struct EventGet : public Event {
	EventGet(double ts, double lat, int size, const KeyRef& in_key)
	  : Event(GET_LATENCY, ts), latency(lat), valueSize(size), key(in_key) {}
	EventGet() {}

	template <typename Ar>
	void serialize(Ar& ar) {
		if (!ar.isDeserializing) {
			old_serializer(ar, Event::base(), latency, valueSize, key);
		} else
			serializer(ar, latency, valueSize, key);
	}

	double latency;
	int valueSize;
	Key key;

	void logEvent(std::string id) const {
		TraceEvent("TransactionTrace_Get")
		    .detail("TransactionID", id)
		    .detail("Latency", latency)
		    .detail("ValueSizeBytes", valueSize)
		    .detail("Key", printable(key));
	}
};

struct EventGetRange : public Event {
	EventGetRange(double ts, double lat, int size, const KeyRef& start_key, const KeyRef& end_key)
	  : Event(GET_RANGE_LATENCY, ts), latency(lat), rangeSize(size), startKey(start_key), endKey(end_key) {}
	EventGetRange() {}

	template <typename Ar>
	void serialize(Ar& ar) {
		if (!ar.isDeserializing) {
			old_serializer(ar, base(), latency, rangeSize, startKey, endKey);
		} else
			serializer(ar, latency, rangeSize, startKey, endKey);
	}

	double latency;
	int rangeSize;
	Key startKey;
	Key endKey;

	void logEvent(std::string id) const {
		TraceEvent("TransactionTrace_GetRange")
		    .detail("TransactionID", id)
		    .detail("Latency", latency)
		    .detail("RangeSizeBytes", rangeSize)
		    .detail("StartKey", printable(startKey))
		    .detail("EndKey", printable(endKey));
	}
};

struct EventCommit : public Event {
	EventCommit(double ts, double lat, int mut, int bytes, const CommitTransactionRequest& commit_req)
	  : Event(COMMIT_LATENCY, ts), latency(lat), numMutations(mut), commitBytes(bytes), req(commit_req) {}
	EventCommit() {}

	template <typename Ar>
	void serialize(Ar& ar) {
		if (!ar.isDeserializing) {
			serializer(ar, base(), latency, numMutations, commitBytes, req.transaction, req.arena);
		} else {
			serializer(ar, latency, numMutations, commitBytes, req.transaction, req.arena);
		}
	}

	double latency;
	int numMutations;
	int commitBytes;
	CommitTransactionRequest
	    req; // Only CommitTransactionRef and Arena object within CommitTransactionRequest is serialized

	void logEvent(std::string id) const {
		for (auto& read_range : req.transaction.read_conflict_ranges) {
			TraceEvent("TransactionTrace_Commit_ReadConflictRange")
			    .detail("TransactionID", id)
			    .detail("Begin", printable(read_range.begin))
			    .detail("End", printable(read_range.end));
		}

		for (auto& write_range : req.transaction.write_conflict_ranges) {
			TraceEvent("TransactionTrace_Commit_WriteConflictRange")
			    .detail("TransactionID", id)
			    .detail("Begin", printable(write_range.begin))
			    .detail("End", printable(write_range.end));
		}

		for (auto& mutation : req.transaction.mutations) {
			TraceEvent("TransactionTrace_Commit_Mutation")
			    .detail("TransactionID", id)
			    .detail("Mutation", mutation.toString());
		}

		TraceEvent("TransactionTrace_Commit")
		    .detail("TransactionID", id)
		    .detail("Latency", latency)
		    .detail("NumMutations", numMutations)
		    .detail("CommitSizeBytes", commitBytes);
	}
};

struct EventGetError : public Event {
	EventGetError(double ts, int err_code, const KeyRef& in_key)
	  : Event(ERROR_GET, ts), errCode(err_code), key(in_key) {}
	EventGetError() {}

	template <typename Ar>
	void serialize(Ar& ar) {
		if (!ar.isDeserializing) {
			serializer(ar, base(), errCode, key);
		} else {
			serializer(ar, errCode, key);
		}
	}

	int errCode;
	Key key;

	void logEvent(std::string id) const {
		TraceEvent("TransactionTrace_GetError")
		    .detail("TransactionID", id)
		    .detail("ErrCode", errCode)
		    .detail("Key", printable(key));
	}
};

struct EventGetRangeError : public Event {
	EventGetRangeError(double ts, int err_code, const KeyRef& start_key, const KeyRef& end_key)
	  : Event(ERROR_GET_RANGE, ts), errCode(err_code), startKey(start_key), endKey(end_key) {}
	EventGetRangeError() {}

	template <typename Ar>
	void serialize(Ar& ar) {
		if (!ar.isDeserializing) {
			serializer(ar, base(), errCode, startKey, endKey);
		} else {
			serializer(ar, errCode, startKey, endKey);
		}
	}

	int errCode;
	Key startKey;
	Key endKey;

	void logEvent(std::string id) const {
		TraceEvent("TransactionTrace_GetRangeError")
		    .detail("TransactionID", id)
		    .detail("ErrCode", errCode)
		    .detail("StartKey", printable(startKey))
		    .detail("EndKey", printable(endKey));
	}
};

struct EventCommitError : public Event {
	EventCommitError(double ts, int err_code, const CommitTransactionRequest& commit_req)
	  : Event(ERROR_COMMIT, ts), errCode(err_code), req(commit_req) {}
	EventCommitError() {}

	template <typename Ar>
	void serialize(Ar& ar) {
		if (!ar.isDeserializing) {
			serializer(ar, base(), errCode, req.transaction, req.arena);
		} else
			serializer(ar, errCode, req.transaction, req.arena);
	}

	int errCode;
	CommitTransactionRequest
	    req; // Only CommitTransactionRef and Arena object within CommitTransactionRequest is serialized

	void logEvent(std::string id) const {
		for (auto& read_range : req.transaction.read_conflict_ranges) {
			TraceEvent("TransactionTrace_CommitError_ReadConflictRange")
			    .detail("TransactionID", id)
			    .detail("Begin", printable(read_range.begin))
			    .detail("End", printable(read_range.end));
		}

		for (auto& write_range : req.transaction.write_conflict_ranges) {
			TraceEvent("TransactionTrace_CommitError_WriteConflictRange")
			    .detail("TransactionID", id)
			    .detail("Begin", printable(write_range.begin))
			    .detail("End", printable(write_range.end));
		}

		for (auto& mutation : req.transaction.mutations) {
			TraceEvent("TransactionTrace_CommitError_Mutation")
			    .detail("TransactionID", id)
			    .detail("Mutation", mutation.toString());
		}

		TraceEvent("TransactionTrace_CommitError").detail("TransactionID", id).detail("ErrCode", errCode);
	}
};

struct SerializableEvent {
	constexpr static flat_buffers::FileIdentifier file_identifier = 14693020;
	using variant = boost::variant<Void, EventGetVersion, EventGet, EventGetRange, EventCommit, EventGetError,
	                               EventGetRangeError, EventCommitError>;

	variant self;

	SerializableEvent() {}

	SerializableEvent(SerializableEvent&& other) : self(std::move(other.self)) {}
	SerializableEvent(const SerializableEvent& other) : self(other.self) {}

	SerializableEvent(variant&& v) : self(std::move(v)) {}
	SerializableEvent(const variant& v) : self(v) {}

	template <class Ar>
	void serialize(Ar& ar) {
		boost::apply_visitor([&ar](auto& val) { old_serializer(ar, val); }, self);
	}
};

} // namespace FdbClientLogEvents

namespace flat_buffers {

template <>
struct serializable_traits<FdbClientLogEvents::SerializableEvent> : std::true_type {
	using type = FdbClientLogEvents::SerializableEvent;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.self);
	}
};

template <>
struct serializable_traits<FdbClientLogEvents::Event> : std::true_type {
	using type = FdbClientLogEvents::Event;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.type, e.startTs);
	}
};

template <>
struct serializable_traits<FdbClientLogEvents::EventGetVersion> : std::true_type {
	using type = FdbClientLogEvents::EventGetVersion;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.base(), e.latency);
	}
};

template <>
struct serializable_traits<FdbClientLogEvents::EventGet> : std::true_type {
	using type = FdbClientLogEvents::EventGet;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.base(), e.latency, e.valueSize, e.key);
	}
};

template <>
struct serializable_traits<FdbClientLogEvents::EventGetRange> : std::true_type {
	using type = FdbClientLogEvents::EventGetRange;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.base(), e.latency, e.rangeSize, e.startKey, e.endKey);
	}
};

template <>
struct serializable_traits<FdbClientLogEvents::EventCommit> : std::true_type {
	using type = FdbClientLogEvents::EventCommit;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.base(), e.latency, e.numMutations, e.commitBytes, e.req.transaction, e.req.arena);
	}
};

template <>
struct serializable_traits<FdbClientLogEvents::EventGetError> : std::true_type {
	using type = FdbClientLogEvents::EventGetError;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.base(), e.errCode, e.key);
	}
};

template <>
struct serializable_traits<FdbClientLogEvents::EventGetRangeError> : std::true_type {
	using type = FdbClientLogEvents::EventGetRangeError;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.base(), e.errCode, e.startKey, e.endKey);
	}
};

template <>
struct serializable_traits<FdbClientLogEvents::EventCommitError> : std::true_type {
	using type = FdbClientLogEvents::EventCommitError;

	template <class Ar>
	static void serialize(Ar& ar, type& e) {
		::serializer(ar, e.base(), e.errCode, e.req.transaction, e.req.arena);
	}
};

} // namespace flat_buffers

#endif
