/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Usage examples:
 *
 * 1) Capturing a weak_ptr in a lambda and locking it at the top
 *    of the lambda body to avoid use-after-free:
 *
 *    // inside a class deriving from std::enable_shared_from_this<Foo>
 *    void start() {
 *      // executor-specific spawn/callback API
 *      someSpawnOrAsyncCall(
 *          [WEAK_SELF] {           // capture weak ptr as 'weak_self'
 *            WEAK_LOCK(self);      // auto self = weak_self.lock(); if (!self) return;
 *            self->doWork();
 *          });
 *    }
 *
 * 2) Using the conditional helper when you only need to touch self briefly:
 *
 *    someCallback([WEAK_SELF] {
 *      IF_WEAK_LOCK(self) {        // if (auto self = weak_self.lock())
 *        self->touch();
 *      }
 *    });
 *
 * Notes:
 * - Requires the enclosing type to inherit std::enable_shared_from_this<T>.
 * - Use [WEAK_SELF] in the lambda capture list; then use WEAK_LOCK(self)
 *   or IF_WEAK_LOCK(self) inside the lambda body.
 */
#define WEAK_SELF          \
  weak_self {              \
    this->weak_from_this() \
  }

#define WEAK_LOCK(name)           \
  auto name = weak_##name.lock(); \
  if (not name) return;

#define IF_WEAK_LOCK(name) if (auto name = weak_##name.lock())
