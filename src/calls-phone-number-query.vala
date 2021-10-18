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
    private string _number;
    private E.PhoneNumber _ephonenumber;
    private string _country_code;

    public PhoneNumberQuery (string number, string? country_code)
    {
        string[] match_fields =
        { Folks.PersonaStore.detail_key (Folks.PersonaDetail.PHONE_NUMBERS) };

        Object (match_fields: match_fields);

        this._number = number;
        this._country_code = country_code;

        try
        {
            this._ephonenumber =
                E.PhoneNumber.from_string (this._number, this._country_code);
        }
        catch (GLib.Error e)
        {
        // Fall back to string comparison in this case
        debug ("Failed to convert `%s' to a phone number: %s",
               this._number,
               e.message);
        }
    }

    public override uint is_match (Folks.Individual individual)
    {
        const uint MATCH_MAX = 4;
        bool use_ephone = this._ephonenumber != null;

        // Iterate over the set of phone numbers
        Gee.Iterator<Folks.PhoneFieldDetails> iter =
            individual.phone_numbers.iterator ();
        uint match = 0;
        while (match < MATCH_MAX && iter.next ())
        {
            // Get the phone number
            Folks.PhoneFieldDetails details = iter.get ();
            string indiv_number = details.value;
            uint this_match = 0;

            if (use_ephone)
            {
                // Parse it
                E.PhoneNumber indiv_parsed;
                try
                {
                    indiv_parsed =
                        E.PhoneNumber.from_string (indiv_number, this._country_code);

                    E.PhoneNumberMatch result =
                    indiv_parsed.compare (this._ephonenumber);

                    switch (result)
                    {
                    case E.PhoneNumberMatch.NONE:     this_match = 0; break;
                    case E.PhoneNumberMatch.SHORT:    this_match = 0; break;
                    case E.PhoneNumberMatch.NATIONAL: this_match = 1; break;
                    case E.PhoneNumberMatch.EXACT:    this_match = MATCH_MAX; break;
                    default:                          this_match = 0; break;
                    }

                }
                catch (GLib.Error e)
                {
                    debug ("Error parsing Folks phone number `%s'" +
                           " for Individual `%s': %s",
                           indiv_number,
                           individual.display_name,
                           e.message);

                    if (this._number == indiv_number)
                    {
                        this_match = MATCH_MAX;
                    }

                }
            }
            else
            {
                // Fall back to string comparison
                if (this._number == indiv_number)
                {
                    this_match = MATCH_MAX;
                }
            }

            if (this_match > match)
            {
                match = this_match;
            }
        }

        return match;
    }
}
