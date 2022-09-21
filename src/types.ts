export interface Elem {
	tag: string
	attrs?: {[name: string]: string}
	content: Array<string | Elem> | string
}

export interface Author {
	id: string,
	name: string
}

export interface SeriesRef {
    seriesTitle: string,
    nextArticle?: {
        id: string,
        title: string
    },
    previousArticle?: {
        id: string,
        title: string
    }
}

export interface AssetCollection {
    stylesheets?: string[],
    scripts?: string[],
    embedded_scripts?: string[]
}

export interface Article {
	id: string,
	title: string,
	author: Author,
	time: Date,
	description: string,
	content: ArticleAst,
    published: boolean
    series?: SeriesRef,
    assets: {head?: AssetCollection, body?: AssetCollection}
    descriptionImg?: string,
    tags: []
}

export interface File {
    fileName: string,
    content: string
}

export interface Assets {
    stylesheets: File[]
}

export interface Blog {
    icon: string,
    blogTitle: string,
    copyrightHolder: string,
    id: string,
	articles: Article[]
	authors: {[id: string]: Author}
    outDir: string,
    settingsTemplate: string,
    articleTemplate: string,
    listTemplate: string,
    redirectTemplate?: string,
    subscribeTemplate?: string,
    assets: Assets,
    fontsFolder?: string,
    imgFolder: string,
    descriptionImg?: string,
    baseUrl: string,
    description: string,
    categories: string[]
}

export enum ArticleTokenType {
    BLOCK_QUOTE,
    BOLD_ITALIC,
    BOLD,
    ITALIC,
    HEADER,
    REF,
    LINK,
    LIST,
    TABLE_OF_CONTENTS,
    PARAGRAPH_BREAK,
    EMPHASIS,
    ELEM,
    CODE_BLOCK,
    REFERENCE,
    PARAGRAPH,
    OBJ_LINK,
}

export interface ArticleToken {
    type: ArticleTokenType
    data: Array<string | ArticleToken>,
    attrs?: {[name: string]: any}
}

export type ArticleReference = {
    id: number,
	type: string
    [prop: string]: string | number
}

export type ArticleReferences = {[refId: string]: ArticleReference};

export type ArticleTokens = Array<string | ArticleToken>

export interface Section {
    title: string,
    level: number
}

export interface ArticleAst {
    tokens: ArticleTokens,
    references: ArticleReferences,
    sections: Section[]
}