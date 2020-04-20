// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <krabs.hpp>
#include "IEventRecordMetadata.hpp"
#include "Guid.hpp"

using namespace System;
using namespace System::Runtime::InteropServices;

namespace Microsoft { namespace O365 { namespace Security { namespace ETW {

    /// <summary>
    /// Concrete implementation representing the metadata about an ETW event record.
    /// </summary>
    public ref class EventRecordMetadata : public IEventRecordMetadata
    {
    protected:
        const EVENT_RECORD* record_;
        const EVENT_HEADER* header_;

    internal:
        EventRecordMetadata(const EVENT_RECORD& record)
            : record_(&record)
            , header_(&record.EventHeader) { }

    public:
#pragma region EventDescriptor

        /// <summary>
        /// Retrieves the ID of this event.
        /// </summary>
        virtual property uint16_t Id
        {
            uint16_t get() { return header_->EventDescriptor.Id; }
        }

        /// <summary>
        /// Returns the opcode of this event.
        /// </summary>
        virtual property byte Opcode
        {
            virtual byte get() { return header_->EventDescriptor.Opcode; }
        }

        /// <summary>
        /// Returns the version of this event.
        /// </summary>
        virtual property byte Version
        {
            byte get() { return header_->EventDescriptor.Version; }
        }

        /// <summary>
        /// Returns the level of this event.
        /// </summary>
        virtual property byte Level
        {
            byte get() { return header_->EventDescriptor.Level; }
        }

#pragma endregion

#pragma region EventHeader

        /// <summary>
        /// Returns the flags of the event.
        /// </summary>
        virtual property uint16_t Flags
        {
            uint16_t get() { return header_->Flags; }
        }

        /// <summary>
        /// Returns the EventProperty of the event.
        /// </summary>
        virtual property EventHeaderProperty EventProperty
        {
            EventHeaderProperty get()
            {
                return (EventHeaderProperty)(header_->EventProperty);
            }
        }

        /// <summary>
        /// Retrieves the PID associated with the event.
        /// </summary>
        virtual property unsigned int ProcessId
        {
            unsigned int get() { return header_->ProcessId; }
        }

        /// <summary>
        /// Retrieves the Thread ID associated with the event.
        /// </summary>
        virtual property unsigned int ThreadId
        {
            unsigned int get() { return header_->ThreadId; }
        }

        /// <summary>
        /// Returns the timestamp associated with this event.
        /// </summary>
        virtual property DateTime Timestamp
        {
            DateTime get()
            {
                return DateTime::FromFileTimeUtc(header_->TimeStamp.QuadPart);
            }
        }

        /// <summary>
        /// Returns the Thread ID associated with the event.
        /// </summary>
        virtual property Guid ProviderId
        {
            Guid get() { return ConvertGuid(header_->ProviderId); }
        }

#pragma endregion

#pragma region EventRecord

        /// <summary>
        /// Returns the size in bytes of the UserData buffer.
        /// </summary>
        /// <returns>the size of the EVENT_RECORD.UserData buffer</returns>
        virtual property uint16_t UserDataLength
        {
            uint16_t get() { return record_->UserDataLength; }
        }

        /// <summary>
        /// Returns a pointer to the UserData buffer.
        /// </summary>
        /// <returns>a pointer to the EVENT_RECORD.UserData buffer</returns>
        virtual property IntPtr UserData
        {
            IntPtr get() { return IntPtr(record_->UserData); }
        }

        /// <summary>
        /// Marshals the event UserData onto the managed heap.
        /// </summary>
        /// <returns>a byte array representing the marshalled EVENT_RECORD.UserData buffer</returns>
        virtual array<uint8_t>^ CopyUserData()
        {
            auto dest = gcnew array<uint8_t>(UserDataLength);
            Marshal::Copy(UserData, dest, 0, UserDataLength);

            return dest;
        }

#pragma endregion

#pragma region ExtendedData

        /// <summary>
        /// If the event's extended data contains an Argon container ID, retrieve it.
        /// Can be expensive, avoid calling more than once per event.
        /// </summary>
        /// <returns>
        /// A Guid representing the Argon container ID if the data is present. Null it's not present. 
        /// Throws a ContainerIdFormatException if the container ID is present but parsing fails.
        /// </returns>
        virtual System::Nullable<System::Guid> GetContainerId()
        {
            // We are expecting format "00000000-0000-0000-0000-0000000000000", 32 hex digits with 4 hyphens
            const size_t CONTAINER_ID_DATA_LENGTH_IN_BYTES = 36;

            size_t extended_data_count = record_->ExtendedDataCount;
            for (size_t i = 0; i < extended_data_count; i++)
            {
                EVENT_HEADER_EXTENDED_DATA_ITEM& extended_data = record_->ExtendedData[i];

                if (extended_data.ExtType == EVENT_HEADER_EXT_TYPE_CONTAINER_ID)
                {
                    // Convert the non-null terminated, no-braces ASCII GUID into a null terminated, curly braces, wide string 
                    // for parsing.
                    assert(extended_data.DataSize == CONTAINER_ID_DATA_LENGTH_IN_BYTES);
                    wchar_t guid_string_buffer[1 + CONTAINER_ID_DATA_LENGTH_IN_BYTES + 2] = L"{00000000-0000-0000-0000-000000000000}";
                    for (size_t c = 0; c < CONTAINER_ID_DATA_LENGTH_IN_BYTES; c++)
                    {
                        guid_string_buffer[c + 1] = (wchar_t)((reinterpret_cast<char*>(extended_data.DataPtr))[c]);
                    }

                    // Parse GUID in native code to avoid marshalling any strings.
                    GUID container_guid;
                    HRESULT guid_conversion_error = CLSIDFromString(guid_string_buffer, &container_guid);
                    if (guid_conversion_error != S_OK)
                    {
                        // As long as we're getting GUIDs in the expected format from the extended data, this shouldn't be 
                        // happening, but if it does it should be explicit instead of making the event look like it's not coming
                        // from inside an Argon container.
                        System::String^ guidData = gcnew System::String(guid_string_buffer);
                        System::Int32 errorCode = System::Int32(guid_conversion_error);
                        throw gcnew ContainerIdFormatException(
                            System::String::Format(
                                "Failed to convert event's container ID data to GUID. Error code: {0}, Data: {1}",
                                errorCode,
                                guidData));
                    }

                    // Convert to managed System::Guid for returning to managed code.
                    return System::Nullable<System::Guid>(ConvertGuid(container_guid));
                }
            }

            // Not found, return null.
            return System::Nullable<System::Guid>();
        }

#pragma endregion
    };

} } } }