/*
 * StorageServerInterface.h
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

#ifndef FDBCLIENT_STORAGESERVERINTERFACE_H
#define FDBCLIENT_STORAGESERVERINTERFACE_H
#pragma once

#include "FDBTypes.h"
#include "fdbrpc/Locality.h"
#include "fdbrpc/QueueModel.h"
#include "fdbrpc/fdbrpc.h"
#include "fdbrpc/LoadBalance.actor.h"

struct StorageServerInterface {
	constexpr static flat_buffers::FileIdentifier file_identifier = 1111273;
	enum { BUSY_ALLOWED = 0, BUSY_FORCE = 1, BUSY_LOCAL = 2 };

	enum { LocationAwareLoadBalance = 1 };

	LocalityData locality;
	UID uniqueID;

	RequestStream<ReplyPromise<Version>> getVersion;
	RequestStream<struct GetValueRequest> getValue;
	RequestStream<struct GetKeyRequest> getKey;

	// Throws a wrong_shard_server if the keys in the request or result depend on data outside this server OR if a large
	// selector offset prevents all data from being read in one range read
	RequestStream<struct GetKeyValuesRequest> getKeyValues;

	RequestStream<struct GetShardStateRequest> getShardState;
	RequestStream<struct WaitMetricsRequest> waitMetrics;
	RequestStream<struct SplitMetricsRequest> splitMetrics;
	RequestStream<struct GetPhysicalMetricsRequest> getPhysicalMetrics;
	RequestStream<ReplyPromise<Void>> waitFailure;
	RequestStream<struct StorageQueuingMetricsRequest> getQueuingMetrics;

	RequestStream<ReplyPromise<KeyValueStoreType>> getKeyValueStoreType;
	RequestStream<struct WatchValueRequest> watchValue;

	explicit StorageServerInterface(UID uid) : uniqueID(uid) {}
	StorageServerInterface() : uniqueID(g_random->randomUniqueID()) {}
	NetworkAddress address() const { return getVersion.getEndpoint().address; }
	UID id() const { return uniqueID; }
	std::string toString() const { return id().shortString(); }
	template <class Ar>
	void serialize(Ar& ar) {
		// StorageServerInterface is persisted in the database and in the tLog's data structures, so changes here have
		// to be versioned carefully!
		if constexpr (only_old_protocol<Ar>) {
			serializer(ar, uniqueID, locality, getVersion, getValue, getKey, getKeyValues, getShardState, waitMetrics,
			           splitMetrics, getPhysicalMetrics, waitFailure, getQueuingMetrics, getKeyValueStoreType);
			if (ar.protocolVersion() >= 0x0FDB00A200090001LL) serializer(ar, watchValue);
		} else {
			serializer(ar, uniqueID, locality, getVersion, getValue, getKey, getKeyValues, getShardState, waitMetrics,
			           splitMetrics, getPhysicalMetrics, waitFailure, getQueuingMetrics, getKeyValueStoreType,
			           watchValue);
		}
	}
	bool operator==(StorageServerInterface const& s) const { return uniqueID == s.uniqueID; }
	bool operator<(StorageServerInterface const& s) const { return uniqueID < s.uniqueID; }
	void initEndpoints() {
		getValue.getEndpoint(TaskLoadBalancedEndpoint);
		getKey.getEndpoint(TaskLoadBalancedEndpoint);
		getKeyValues.getEndpoint(TaskLoadBalancedEndpoint);
	}
};

struct StorageInfo : NonCopyable, public ReferenceCounted<StorageInfo> {
	Tag tag;
	StorageServerInterface interf;
	StorageInfo() : tag(invalidTag) {}
};

struct ServerCacheInfo {
	std::vector<Tag> tags;
	std::vector<Reference<StorageInfo>> src_info;
	std::vector<Reference<StorageInfo>> dest_info;
};

struct GetValueReply : public LoadBalancedReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 12028881;
	Optional<Value> value;

	GetValueReply() {}
	GetValueReply(Optional<Value> value) : value(value) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, *static_cast<LoadBalancedReply*>(this), value);
	}
};

struct GetValueRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 8363286;

	Key key;
	Version version;
	Optional<UID> debugID;
	ReplyPromise<GetValueReply> reply;

	GetValueRequest() {}
	GetValueRequest(const Key& key, Version ver, Optional<UID> debugID) : key(key), version(ver), debugID(debugID) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, key, version, debugID, reply);
	}
};

struct WatchValueRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 9659512;

	Key key;
	Optional<Value> value;
	Version version;
	Optional<UID> debugID;
	ReplyPromise<Version> reply;

	WatchValueRequest() {}
	WatchValueRequest(const Key& key, Optional<Value> value, Version ver, Optional<UID> debugID)
	  : key(key), value(value), version(ver), debugID(debugID) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, key, value, version, debugID, reply);
	}
};

struct GetKeyValuesReply : public LoadBalancedReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 10565888;

	Arena arena;
	VectorRef<KeyValueRef> data;
	Version version; // useful when latestVersion was requested
	bool more;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, *static_cast<LoadBalancedReply*>(this), data, version, more, arena);
	}
};

struct GetKeyValuesRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 4041376;

	Arena arena;
	KeySelectorRef begin, end;
	Version version; // or latestVersion
	int limit, limitBytes;
	Optional<UID> debugID;
	ReplyPromise<GetKeyValuesReply> reply;

	GetKeyValuesRequest() {}
	//	GetKeyValuesRequest(const KeySelectorRef& begin, const KeySelectorRef& end, Version version, int limit, int
	// limitBytes, Optional<UID> debugID) : begin(begin), end(end), version(version), limit(limit),
	// limitBytes(limitBytes) {}
	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, begin, end, version, limit, limitBytes, debugID, reply, arena);
	}
};

struct GetKeyReply : public LoadBalancedReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 47266;

	KeySelector sel;

	GetKeyReply() {}
	GetKeyReply(KeySelector sel) : sel(sel) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, *static_cast<LoadBalancedReply*>(this), sel);
	}
};

struct GetKeyRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 8812209;

	Arena arena;
	KeySelectorRef sel;
	Version version; // or latestVersion
	ReplyPromise<GetKeyReply> reply;

	GetKeyRequest() {}
	GetKeyRequest(KeySelectorRef const& sel, Version version) : sel(sel), version(version) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, sel, version, reply, arena);
	}
};

struct GetShardStateRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 15560798;

	enum waitMode { NO_WAIT = 0, FETCHING = 1, READABLE = 2 };

	KeyRange keys;
	int32_t mode;
	ReplyPromise<std::pair<Version, Version>> reply;
	GetShardStateRequest() {}
	GetShardStateRequest(KeyRange const& keys, waitMode mode) : keys(keys), mode(mode) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, keys, mode, reply);
	}
};

struct StorageMetrics {
	constexpr static flat_buffers::FileIdentifier file_identifier = 14813438;

	int64_t bytes; // total storage
	int64_t bytesPerKSecond; // network bandwidth (average over 10s)
	int64_t iosPerKSecond;

	static const int64_t infinity = 1LL << 60;

	StorageMetrics() : bytes(0), bytesPerKSecond(0), iosPerKSecond(0) {}

	bool allLessOrEqual(const StorageMetrics& rhs) const {
		return bytes <= rhs.bytes && bytesPerKSecond <= rhs.bytesPerKSecond && iosPerKSecond <= rhs.iosPerKSecond;
	}
	void operator+=(const StorageMetrics& rhs) {
		bytes += rhs.bytes;
		bytesPerKSecond += rhs.bytesPerKSecond;
		iosPerKSecond += rhs.iosPerKSecond;
	}
	void operator-=(const StorageMetrics& rhs) {
		bytes -= rhs.bytes;
		bytesPerKSecond -= rhs.bytesPerKSecond;
		iosPerKSecond -= rhs.iosPerKSecond;
	}
	template <class F>
	void operator*=(F f) {
		bytes *= f;
		bytesPerKSecond *= f;
		iosPerKSecond *= f;
	}
	bool allZero() const { return !bytes && !bytesPerKSecond && !iosPerKSecond; }

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, bytes, bytesPerKSecond, iosPerKSecond);
	}

	void negate() { operator*=(-1.0); }
	StorageMetrics operator-() const {
		StorageMetrics x(*this);
		x.negate();
		return x;
	}
	StorageMetrics operator+(const StorageMetrics& r) const {
		StorageMetrics x(*this);
		x += r;
		return x;
	}
	StorageMetrics operator-(const StorageMetrics& r) const {
		StorageMetrics x(r);
		x.negate();
		x += *this;
		return x;
	}
	template <class F>
	StorageMetrics operator*(F f) const {
		StorageMetrics x(*this);
		x *= f;
		return x;
	}

	bool operator==(StorageMetrics const& rhs) const {
		return bytes == rhs.bytes && bytesPerKSecond == rhs.bytesPerKSecond && iosPerKSecond == rhs.iosPerKSecond;
	}

	std::string toString() const {
		return format("Bytes: %lld, BPerKSec: %lld, iosPerKSec: %lld", bytes, bytesPerKSecond, iosPerKSecond);
	}
};

struct WaitMetricsRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 2636458;
	// Waits for any of the given minimum or maximum metrics to be exceeded, and then returns the current values
	// Send a reversed range for min, max to receive an immediate report
	Arena arena;
	KeyRangeRef keys;
	StorageMetrics min, max;
	ReplyPromise<StorageMetrics> reply;

	WaitMetricsRequest() {}
	WaitMetricsRequest(KeyRangeRef const& keys, StorageMetrics const& min, StorageMetrics const& max)
	  : keys(arena, keys), min(min), max(max) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, keys, min, max, reply, arena);
	}
};

struct SplitMetricsReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 10456489;
	Standalone<VectorRef<KeyRef>> splits;
	StorageMetrics used;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, splits, used);
	}
};

struct SplitMetricsRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 9182023;

	Arena arena;
	KeyRangeRef keys;
	StorageMetrics limits;
	StorageMetrics used;
	StorageMetrics estimated;
	bool isLastShard;
	ReplyPromise<SplitMetricsReply> reply;

	SplitMetricsRequest() {}
	SplitMetricsRequest(KeyRangeRef const& keys, StorageMetrics const& limits, StorageMetrics const& used,
	                    StorageMetrics const& estimated, bool isLastShard)
	  : keys(arena, keys), limits(limits), used(used), estimated(estimated), isLastShard(isLastShard) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, keys, limits, used, estimated, isLastShard, reply, arena);
	}
};

struct GetPhysicalMetricsReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 10096600;

	StorageMetrics load;
	StorageMetrics free;
	StorageMetrics capacity;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, load, free, capacity);
	}
};

struct GetPhysicalMetricsRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 6896372;

	ReplyPromise<GetPhysicalMetricsReply> reply;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, reply);
	}
};

struct StorageQueuingMetricsRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 12815316;
	// SOMEDAY: Send threshold value to avoid polling faster than the information changes?
	ReplyPromise<struct StorageQueuingMetricsReply> reply;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, reply);
	}
};

struct StorageQueuingMetricsReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 7934839;

	double localTime;
	int64_t instanceID; // changes if bytesDurable and bytesInput reset
	int64_t bytesDurable, bytesInput;
	StorageBytes storageBytes;
	Version v; // current storage server version

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, localTime, instanceID, bytesDurable, bytesInput, v, storageBytes);
	}
};

#endif
