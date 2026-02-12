/*
 * Copyright (C) 2017-2021, Brown University, Secure Systems Lab.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Brown University nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SYSFILTER_CALL_GRAPH_H
#define SYSFILTER_CALL_GRAPH_H

#include <map>
#include <set>

#include "log.h"

class Function;

// typedef std::map<Function *, std::set<Function *>> Callgraph;

template <typename T>
class extended_set {
public:
    class const_iterator {
    public:
        enum wrapping { A, B };

    private:
        const extended_set<T> &set;
        wrapping which;
        typename std::set<T>::iterator wrap;

    public:
        typedef T value_type;
        typedef int difference_type;
        typedef T *pointer;
        typedef T &reference;
        typedef std::forward_iterator_tag iterator_category;

        const_iterator(const extended_set<T> &set, wrapping which,
            const typename std::set<T>::iterator &wrap)
            : set(set), which(which), wrap(wrap) { }

        T const operator*() { return *wrap; }
        const const_iterator &operator++() {
            if (which == A && wrap == set.a.end()) {
                which = B;
                wrap = set.b.begin();
            }
            if (wrap == set.b.end()) return *this;
            wrap++;
            if (which == A && wrap == set.a.end()) {
                which = B;
                wrap = set.b.begin();
            }

            return *this;
        }

        bool operator==(const const_iterator &other) const {
            return which == other.which && wrap == other.wrap;
        }
        bool operator!=(const const_iterator &other) const {
            return which != other.which || wrap != other.wrap;
        }
    };

private:
    std::set<T> &a;
    const std::set<T> &b;

public:
    extended_set(std::set<T> &a, const std::set<T> &b) : a(a), b(b) { }

    const_iterator find(const T &t) const {
        auto it = a.find(t);
        if (it != a.cend()) return const_iterator(*this, const_iterator::A, it);
        it = b.find(t);
        if (it != b.cend()) return const_iterator(*this, const_iterator::B, it);
        return const_iterator(const_iterator::B, b.cend());
    }

    const_iterator begin() const {
        if (a.size() == 0)
            return const_iterator(*this, const_iterator::B, b.begin());
        else
            return const_iterator(*this, const_iterator::A, a.begin());
    }
    const_iterator end() const {
        return const_iterator(*this, const_iterator::B, b.end());
    }

    void insert(const T &data) { a.insert(data); }
    size_t count(const T &data) { return a.count(data) | b.count(data); }

    extended_set<T> &operator=(std::set<T> &&data) {
        a = data;
        return *this;
    }

    extended_set<T> &operator=(const std::set<T> &data) {
        a = data;
        return *this;
    }
};

class Callgraph {
    friend class CallgraphWriter;

private:
    std::map<Function *, std::set<Function *>> wrap;

    std::set<Function *> implicitSources;
    std::set<Function *> implicitTargets;
    std::set<Function *> emptySet;
    std::set<Function *> keys;
    bool hasKeys = false;

public:
    std::map<Function *, std::set<Function *>> &getDirectEdges() {
        return wrap;
    }
    const std::set<Function *> &getImplicitSources() const {
        return implicitSources;
    }
    const std::set<Function *> &getImplicitTargets() const {
        return implicitTargets;
    }
    void setImplicit(
        std::set<Function *> &&sources, std::set<Function *> &&targets) {
        implicitSources = sources;
        implicitTargets = targets;
    }

    extended_set<Function *> operator[](Function *which) {
        if (implicitSources.count(which)) {
            return extended_set<Function *>(wrap[which], implicitTargets);
        }
        else {
            return extended_set<Function *>(wrap[which], emptySet);
        }
    }

    const std::set<Function *> &getKeys() {
        if (!hasKeys) {
            for (auto e : wrap) {
                keys.insert(e.first);
            }
            hasKeys = true;
        }

        return this->keys;
    }

    void clearKeys() {
        keys.clear();
        hasKeys = false;
    }

    void removeEdge(Function *src, Function *target) {
        // If there is an explicit src->dst edge, remove it
        if (wrap.count(src)) { wrap[src].erase(target); }

        // If target is an implicit target, remove it
        if (implicitTargets.count(target)) { implicitTargets.erase(target); }
        hasKeys = false;
    }

    size_t count(Function *key) {
        return wrap.count(key) + implicitSources.count(key);
    }

    std::set<Function *> &getImplicitSources() { return implicitSources; }

    std::set<Function *> &getImplicitTargets() { return implicitTargets; }

    bool hasDirectEdge(Function *func) const { return (wrap.count(func) != 0); }

    bool isImplicitSource(Function *func) const {
        return (implicitSources.count(func) != 0);
    }

    bool isImplicitTarget(Function *func) const {
        return (implicitTargets.count(func) != 0);
    }
};

#endif
