/*
 * ClusterRecruitmentInterface.h
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

#ifndef FDBSERVER_CLUSTERRECRUITMENTINTERFACE_H
#define FDBSERVER_CLUSTERRECRUITMENTINTERFACE_H
#pragma once

#include "fdbclient/ClusterInterface.h"
#include "fdbclient/StorageServerInterface.h"
#include "fdbclient/MasterProxyInterface.h"
#include "fdbclient/DatabaseConfiguration.h"
#include "MasterInterface.h"
#include "RecoveryState.h"
#include "TLogInterface.h"
#include "WorkerInterface.h"
#include "Knobs.h"

// This interface and its serialization depend on slicing, since the client will deserialize only the first part of this
// structure
struct ClusterControllerFullInterface {
	constexpr static flat_buffers::FileIdentifier file_identifier = 7078912;

	ClusterInterface clientInterface;
	RequestStream<struct RecruitFromConfigurationRequest> recruitFromConfiguration;
	RequestStream<struct RecruitRemoteFromConfigurationRequest> recruitRemoteFromConfiguration;
	RequestStream<struct RecruitStorageRequest> recruitStorage;
	RequestStream<struct RegisterWorkerRequest> registerWorker;
	RequestStream<struct GetWorkersRequest> getWorkers;
	RequestStream<struct RegisterMasterRequest> registerMaster;
	RequestStream<struct GetServerDBInfoRequest> getServerDBInfo;

	UID id() const { return clientInterface.id(); }
	bool operator==(ClusterControllerFullInterface const& r) const { return id() == r.id(); }
	bool operator!=(ClusterControllerFullInterface const& r) const { return id() != r.id(); }

	void initEndpoints() {
		clientInterface.initEndpoints();
		recruitFromConfiguration.getEndpoint(TaskClusterController);
		recruitRemoteFromConfiguration.getEndpoint(TaskClusterController);
		recruitStorage.getEndpoint(TaskClusterController);
		registerWorker.getEndpoint(TaskClusterController);
		getWorkers.getEndpoint(TaskClusterController);
		registerMaster.getEndpoint(TaskClusterController);
		getServerDBInfo.getEndpoint(TaskClusterController);
	}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, clientInterface, recruitFromConfiguration, recruitRemoteFromConfiguration, recruitStorage,
		           registerWorker, getWorkers, registerMaster, getServerDBInfo);
	}
};

struct RecruitFromConfigurationRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 5491855;

	DatabaseConfiguration configuration;
	bool recruitSeedServers;
	int maxOldLogRouters;
	ReplyPromise<struct RecruitFromConfigurationReply> reply;

	RecruitFromConfigurationRequest() {}
	explicit RecruitFromConfigurationRequest(DatabaseConfiguration const& configuration, bool recruitSeedServers,
	                                         int maxOldLogRouters)
	  : configuration(configuration), recruitSeedServers(recruitSeedServers), maxOldLogRouters(maxOldLogRouters) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, configuration, recruitSeedServers, maxOldLogRouters, reply);
	}
};

struct RecruitFromConfigurationReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 2250652;

	vector<WorkerInterface> tLogs;
	vector<WorkerInterface> satelliteTLogs;
	vector<WorkerInterface> proxies;
	vector<WorkerInterface> resolvers;
	vector<WorkerInterface> storageServers;
	vector<WorkerInterface> oldLogRouters;
	Optional<Key> dcId;
	bool satelliteFallback;

	RecruitFromConfigurationReply() : satelliteFallback(false) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, tLogs, satelliteTLogs, proxies, resolvers, storageServers, oldLogRouters, dcId,
		           satelliteFallback);
	}
};

struct RecruitRemoteFromConfigurationRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 16769483;

	DatabaseConfiguration configuration;
	Optional<Key> dcId;
	int logRouterCount;
	ReplyPromise<struct RecruitRemoteFromConfigurationReply> reply;

	RecruitRemoteFromConfigurationRequest() {}
	RecruitRemoteFromConfigurationRequest(DatabaseConfiguration const& configuration, Optional<Key> const& dcId,
	                                      int logRouterCount)
	  : configuration(configuration), dcId(dcId), logRouterCount(logRouterCount) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, configuration, dcId, logRouterCount, reply);
	}
};

struct RecruitRemoteFromConfigurationReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 1841390;

	vector<WorkerInterface> remoteTLogs;
	vector<WorkerInterface> logRouters;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, remoteTLogs, logRouters);
	}
};

struct RecruitStorageReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 5311744;

	WorkerInterface worker;
	ProcessClass processClass;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, worker, processClass);
	}
};

struct RecruitStorageRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 5698442;

	std::vector<TOptional<Standalone<StringRef>>> excludeMachines; //< Don't recruit any of these machines
	std::vector<AddressExclusion> excludeAddresses; //< Don't recruit any of these addresses
	std::vector<TOptional<Standalone<StringRef>>> includeDCs;
	bool criticalRecruitment; //< True if machine classes are to be ignored
	ReplyPromise<RecruitStorageReply> reply;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, excludeMachines, excludeAddresses, includeDCs, criticalRecruitment, reply);
	}
};

struct RegisterWorkerReply {
	constexpr static flat_buffers::FileIdentifier file_identifier = 6565314;

	ProcessClass processClass;
	ClusterControllerPriorityInfo priorityInfo;

	RegisterWorkerReply()
	  : priorityInfo(ProcessClass::UnsetFit, false, ClusterControllerPriorityInfo::FitnessUnknown) {}
	RegisterWorkerReply(ProcessClass processClass, ClusterControllerPriorityInfo priorityInfo)
	  : processClass(processClass), priorityInfo(priorityInfo) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, processClass, priorityInfo);
	}
};

struct RegisterWorkerRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 9839170;

	WorkerInterface wi;
	ProcessClass initialClass;
	ProcessClass processClass;
	ClusterControllerPriorityInfo priorityInfo;
	Generation generation;
	ReplyPromise<RegisterWorkerReply> reply;

	RegisterWorkerRequest()
	  : priorityInfo(ProcessClass::UnsetFit, false, ClusterControllerPriorityInfo::FitnessUnknown) {}
	RegisterWorkerRequest(WorkerInterface wi, ProcessClass initialClass, ProcessClass processClass,
	                      ClusterControllerPriorityInfo priorityInfo, Generation generation)
	  : wi(wi), initialClass(initialClass), processClass(processClass), priorityInfo(priorityInfo),
	    generation(generation) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, wi, initialClass, processClass, priorityInfo, generation, reply);
	}
};

struct GetWorkersRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 13693358;
	enum { TESTER_CLASS_ONLY = 0x1, NON_EXCLUDED_PROCESSES_ONLY = 0x2 };

	int flags;
	ReplyPromise<vector<std::pair<WorkerInterface, ProcessClass>>> reply;

	GetWorkersRequest() : flags(0) {}
	explicit GetWorkersRequest(int fl) : flags(fl) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, flags, reply);
	}
};

struct RegisterMasterRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 11195350;

	Standalone<StringRef> dbName;
	UID id;
	LocalityData mi;
	LogSystemConfig logSystemConfig;
	vector<MasterProxyInterface> proxies;
	vector<ResolverInterface> resolvers;
	DBRecoveryCount recoveryCount;
	int64_t registrationCount;
	Optional<DatabaseConfiguration> configuration;
	vector<UID> priorCommittedLogServers;
	RecoveryState recoveryState;
	bool recoveryStalled;

	ReplyPromise<Void> reply;

	RegisterMasterRequest() {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, dbName, id, mi, logSystemConfig, proxies, resolvers, recoveryCount, registrationCount,
		           configuration, priorCommittedLogServers, recoveryState, recoveryStalled, reply);
	}
};

struct GetServerDBInfoRequest {
	constexpr static flat_buffers::FileIdentifier file_identifier = 15675222;

	UID knownServerInfoID;
	Standalone<StringRef> issues;
	std::vector<NetworkAddress> incompatiblePeers;
	ReplyPromise<struct ServerDBInfo> reply;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, knownServerInfoID, issues, incompatiblePeers, reply);
	}
};

#include "ServerDBInfo.h" // include order hack

#endif
