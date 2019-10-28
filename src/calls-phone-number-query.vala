/*
 * Copyright (C) 2019 Purism SPC
 *
 * This file is part of Calls.
 *
 * Calls is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Calls is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Bob Ham <bob.ham@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */


public class Calls.PhoneNumberQuery : Folks.Query
{
    private E.PhoneNumber _number;

    public PhoneNumberQuery (E.PhoneNumber number)
    {
        string[] match_fields =
        { Folks.PersonaStore.detail_key (Folks.PersonaDetail.PHONE_NUMBERS) };

        Object (match_fields: match_fields);

        this._number = number;
    }

    public override uint is_match (Folks.Individual individual)
    {
        const uint MATCH_MAX = 4;

        // Iterate over the set of phone numbers
        Gee.Iterator<Folks.PhoneFieldDetails> iter =
            individual.phone_numbers.iterator ();
        uint match = 0;
        while (match < MATCH_MAX && iter.next ())
        {
            // Get the phone number
            Folks.PhoneFieldDetails details = iter.get ();
            string indiv_number = details.value;

            // Parse it
            E.PhoneNumber indiv_parsed;
            try
            {
                indiv_parsed =
                    E.PhoneNumber.from_string (indiv_number, null);
            }
            catch (GLib.Error e)
            {
                warning ("Error parsing Folks phone number `%s'" +
                         " for Individual `%s': %s",
                         indiv_number,
                         individual.display_name,
                         e.message);
                continue;
            }

            // Compare the Individual's and query's numbers
            E.PhoneNumberMatch result =
                indiv_parsed.compare (this._number);

            uint this_match;
            switch (result)
            {
            case E.PhoneNumberMatch.NONE:     this_match = 0; break;
            case E.PhoneNumberMatch.SHORT:    this_match = 0; break;
            case E.PhoneNumberMatch.NATIONAL: this_match = 1; break;
            case E.PhoneNumberMatch.EXACT:    this_match = MATCH_MAX; break;
            default:                          this_match = 0; break;
            }

            if (this_match > match)
            {
                match = this_match;
            }
        }

        return match;
    }
}
