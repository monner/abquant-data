# -*- coding: utf-8 -*-

from __future__ import annotations
import abc
from typing import List
from abquant.helper import time_counter
from abquant.config import Setting
from abquant.data.tdx_api import get_stock_day
from abquant.utils.logger import system_log as slog

# from abquant.data.tdx import Stock, Future


class ISecurity(object, metaclass=abc.ABCMeta):
    """
    The ISecurity interface declares an `accept` method that should take the
    base ISecurityVisitor interface as an argument.
    """

    @abc.abstractmethod
    def accept(self, visitor: ISecurityVisitor) -> None:
        pass

    def getClassName(self):
        return self.__class__.__name__


class ISecurityVisitor(object, metaclass=abc.ABCMeta):
    @abc.abstractmethod
    def create_day(self, iSecurity: ISecurity):
        pass

    @abc.abstractmethod
    def create_min(self, iSecurity: ISecurity):
        pass

    @abc.abstractmethod
    def create_xdxr(self, iSecurity: ISecurity):
        pass


class SecurityVisitor(ISecurityVisitor):
    def __init__(self, *args, **kwargs):
        "docstring"
        pass

    def create_day(self, iSecurity: ISecurity):
        if getattr(iSecurity, "create_day"):
            iSecurity.create_day(stockOrIndex="index")
            iSecurity.create_day()

    def create_min(self, iSecurity: ISecurity):
        if getattr(iSecurity, "create_min"):
            iSecurity.create_min(stockOrIndex="index")
            iSecurity.create_min()

    def create_xdxr(self, iSecurity: ISecurity):
        if getattr(iSecurity, "create_xdxr", None):
            pass
            # please manually `abquant stock xdxr`
            # iSecurity.create_xdxr()


def get_broker(broker="tdx"):
    """Factory"""
    if broker in ["tdx"]:
        from . import tdx as TDX
    if broker in ["ths"]:
        from . import ths as THS
    brokers = {
        "tdx": TDX,
    }
    return brokers[broker]


@time_counter
def create_base():
    def registe_securities(
        securities: List[ISecurity], visitor: ISecurityVisitor
    ) -> None:
        for sec in securities:
            sec.accept(visitor)

    broker = get_broker()
    securities = [broker.Stock(), broker.Future()]
    registe_securities(securities, SecurityVisitor())


@time_counter
def create_stock_day(codes):
    broker = get_broker()
    s = broker.Stock(codes=codes)
    s.create_day(stockOrIndex="index")
    s.create_day(stockOrIndex="stock")


@time_counter
def create_stock_min(codes, freqs):
    broker = get_broker()
    s = broker.Stock(codes=codes, freqs=freqs)
    s.create_min(stockOrIndex="index")
    s.create_min(stockOrIndex="stock")


@time_counter
def create_stock_xdxr(codes):
    broker = get_broker()
    s = broker.Stock(codes=codes)
    s.create_xdxr()
