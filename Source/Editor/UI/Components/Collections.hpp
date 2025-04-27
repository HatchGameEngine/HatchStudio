#pragma once

#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

template <class T>
class List {
public:
    T* Items = NULL;

    List(int capacity = 1) {
        internal_Count = 0;
        internal_Capacity = capacity;
        Items = (T*)malloc(internal_Capacity * sizeof(T));
    }
    virtual ~List() {
        free(Items);
    }

    T& operator[](int index) {
        return Items[index];
    }

    virtual void   Resize(int count) {
        EnsureCapacity(count);
        internal_Count = count;
    }
    virtual void   Add(T item) {
        EnsureCapacity(internal_Count + 1);
        Items[internal_Count] = item;
        internal_Count++;
    }
    virtual void   Insert(int index, T item) {
        EnsureCapacity(internal_Count + 1);

        for (int i = internal_Count - 1; i >= index; i--)
            Items[i + 1] = Items[i];

        Items[index] = item;
        internal_Count++;
    }
    virtual void   Remove(T item) {
        try {
            int index = IndexOf(item);
            RemoveAt(index);
        }
        catch (CString) { }
    }
    virtual void   RemoveAt(int index) {
        for (int i = index; i < internal_Count; i++)
            Items[i] = Items[i + 1];

        internal_Count--;
    }
    virtual void   RemoveAllMatching(T& match) {
        // int freeIndex = 0;
        // int removed = 0;
        // for (int i = 0; i < internal_Count; i++) {
        //     if (Items[i] != match) {
        //         // This is not a removeable item.
        //         // Move this to the free index.
        //         // Onto the next one~
        //         Items[freeIndex++] = Items[i];
        //     }
        //     else {
        //         // This is a removeable item.
        //         removed++;
        //     }
        // }
        // internal_Count -= removed;
    }
    virtual void   Clear() {
        internal_Count = 0;
    }
    virtual int    IndexOf(T& item) {
        /*for (int i = 0; i < internal_Count; i++)
            if (Items[i] == item)
                return i;*/

        throw "Could not find item in List.";
    }
    virtual bool   Contains(T& item) {
        try {
            IndexOf(item);
            return true;
        }
        catch (CString) { }

        return false;
    }
    virtual int    Count() {
        return internal_Count;
    }

protected:
    int internal_Count;
    int internal_Capacity;
    virtual bool IsFixedSize() const noexcept { return false; };
    virtual bool IsReadOnly() const noexcept { return false; };

    virtual void Resize() {
        internal_Capacity <<= 1;

        if (!Items)
            throw "Null reference to Items";

        Items = (T*)realloc(Items, internal_Capacity * sizeof(T));
        if (!Items)
            throw "Not enough memory available for List resize.";
    }
    virtual void EnsureCapacity(int count) {
        if (count >= internal_Capacity)
            Resize();
    }
};
