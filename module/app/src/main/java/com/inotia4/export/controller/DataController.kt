package com.inotia4.export.controller

import com.inotia4.export.StaticData
import com.yanzhenjie.andserver.annotation.GetMapping
import com.yanzhenjie.andserver.annotation.PathVariable
import com.yanzhenjie.andserver.annotation.RequestMapping
import com.yanzhenjie.andserver.annotation.RequestParam
import com.yanzhenjie.andserver.annotation.RestController

@RestController
@RequestMapping("/api/data")
class DataController {

    @GetMapping("/roles")
    fun roles(): String = table("CHARCLASSBASE")

    @GetMapping("/items")
    fun items(): String = table("ITEMDATABASE")

    @GetMapping("/skills")
    fun skills(): String = table("SKILLDESCBASE")

    @GetMapping("/mercenaries")
    fun mercenaries(): String = table("MERCENARYINFOBASE")

    @GetMapping("/maps")
    fun maps(): String = table("MAPINFOBASE")

    @GetMapping("/monsters")
    fun monsters(): String = table("MONDATABASE")

    @GetMapping("/quests")
    fun quests(): String = table("QUESTINFOBASE")

    @GetMapping("/npcs")
    fun npcs(): String = table("NPCINFOBASE")

    @GetMapping("/events")
    fun events(): String = StaticData.read("reverse/events.json") ?: NOT_FOUND

    @GetMapping("/text")
    fun text(@RequestParam("lang") lang: String): String =
        StaticData.read("text/${lang}.json") ?: NOT_FOUND

    @GetMapping("/tables/{name}")
    fun table(@PathVariable("name") name: String): String =
        readTable(name.uppercase())

    private fun readTable(name: String): String =
        StaticData.read("tables/${name}.json") ?: NOT_FOUND

    private companion object {
        const val NOT_FOUND = "{\"error\":\"not found\"}"
    }
}
