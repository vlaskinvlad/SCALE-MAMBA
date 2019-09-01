from Compiler.types import (
    sint,
    cint,
    regint,
)
from Compiler.library import (
    open_channel,
    close_channel,
)


class ChannelManager(object):
    def __init__(
            self,
            player_dict,
            channel_dict
    ):
        self.players = player_dict
        self.channels = channel_dict

    def private_input(
            self,
            player_name,
            channel_name
    ):
        return sint.get_private_input_from(
            self.players[player_name],
            self.channels[channel_name]
        )

    def n_private_inputs(
            self,
            n,
            player_name,
            channel_name
    ):
        i = 0
        while i < n:
            yield self.private_input(player_name, channel_name)
            i += 1

    def reveal(
            self,
            s,
            player_names=None,
            channel_name='output',
    ):
        if not player_names:
            player_names = self.players.keys()
        if isinstance(player_names, str):
            player_names = [player_names]
        for player_name in player_names:
            s.reveal_to(
                self.players[player_name],
                self.channels[channel_name]
            )

    def public_cint_input(
            self,
            channel_name
    ):
        return cint.public_input(self.channels[channel_name])

    def n_public_cint_inputs(
            self,
            n,
            channel_name
    ):
        i = 0
        while i < n:
            yield self.public_cint_input(channel_name)
            i += 1

    def public_cint_output(
            self,
            c,
            channel_name
    ):
        return c.public_output(self.channels[channel_name])

    def public_regint_input(
            self,
            channel_name
    ):
        return regint.public_input(self.channels[channel_name])

    def n_public_regint_inputs(
            self,
            n,
            channel_name
    ):
        i = 0
        while i < n:
            yield self.public_regint_input(channel_name)
            i += 1

    def public_regint_output(
            self,
            r,
            channel_name
    ):
        return r.public_output(self.channels[channel_name])

    def __enter__(self):
        for channel in self.channels.values():
            open_channel(channel)
        return self

    def __exit__(self, type, value, traceback):
        for channel in self.channels.values():
            close_channel(channel)
