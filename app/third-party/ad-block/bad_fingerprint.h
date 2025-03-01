/*
    SPDX-License-Identifier: MPL-2.0
*/

/* Copyright (c) 2015 Brian R. Bondy. Distributed under the MPL2 license.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAD_FINGERPRINT_H_
#define BAD_FINGERPRINT_H_

#include <string.h>
#include <math.h>
#include "./hash_set.h"

#ifdef PERF_STATS
#include <fstream>
#endif

class BadFingerprint {
 public:
  uint64_t GetHash() const {
    return 0;
  }

  ~BadFingerprint() {
    if (data) {
      delete[] data;
    }
  }
  explicit BadFingerprint(const char *data) {
    size_t len = strlen(data) + 1;
    this->data = new char[len];
    snprintf(this->data, len, "%s", data);
  }

  BadFingerprint(const BadFingerprint &rhs) {
    data = new char[strlen(rhs.data) + 1];
    memcpy(data, rhs.data, strlen(rhs.data) + 1);
  }

  BadFingerprint() : data(nullptr) {
  }

  bool operator==(const BadFingerprint &rhs) const {
    return !strcmp(data, rhs.data);
  }

  bool operator!=(const BadFingerprint &rhs) const {
    return !(*this == rhs);
  }

  // Nothing needs to be updated for multiple adds
  void Update(const BadFingerprint &) {}

  uint32_t Serialize(char *buffer) {
    if (buffer) {
      memcpy(buffer, data, strlen(data) + 1);
    }
    return static_cast<uint32_t>(strlen(data)) + 1;
  }

  uint32_t Deserialize(char *buffer, uint32_t bufferSize) {
    uint32_t len = static_cast<uint32_t>(strlen(buffer));
    data = new char[len + 1];
    memcpy(data, buffer, len + 1);
    return len + 1;
  }

  char *data;
};

class BadFingerprintsHashSet : public HashSet<BadFingerprint> {
 public:
  BadFingerprintsHashSet() : HashSet<BadFingerprint>(1, false) {
  }

  void generateHeader(const char *filename) {
#ifdef PERF_STATS
    std::ofstream outFile;
    outFile.open(filename);

    outFile << "#pragma once\n";
    outFile << "/**\n  *\n  * Auto generated bad filters\n  */\n";
    outFile << "const char *badFingerprints[] = {\n";
    for (uint32_t bucket_index = 0; bucket_index < bucket_count_;
        bucket_index++) {
      HashItem<BadFingerprint> *hashItem = buckets_[bucket_index];
      while (hashItem) {
        BadFingerprint *badFingerprint = hashItem->hash_item_storage_;
        outFile << "\"" << badFingerprint->data << "\"," << std::endl;
        hashItem = hashItem->next_;
      }
    }
    outFile << "};\n" << std::endl;
    outFile << "const char *badSubstrings[] = {\"http\", \"www\" };"
      << std::endl;
    outFile.close();
    #endif
  }
};

#endif  // BAD_FINGERPRINT_H_
